# Speedrunning mechanics

Why the movement techniques used in LBA2 runs work, read from the engine rather than inferred
from play. Written for the speedrunning community, which is still active, and for anyone changing
input or animation code in this port.

Truth hierarchy: code > this document > external sources. Line numbers were verified against the
working tree; re-grep before relying on an exact one.

**This doubles as a specification.** Every behaviour below is depended on by people, so it is
regression surface. Several are original bugs, and the comments prove the 1997 team knew. They
stay bugs: [BIT_EXACTNESS.md](BIT_EXACTNESS.md) decides what may change in this port, and for
anything here the answer is nothing.

**Attribution.** The French comments quoted below are from the original Adeline codebase and
reflect the 1990s team, not this community, following the practice in
[FRENCH_COMMENTS.md](FRENCH_COMMENTS.md). Techniques credited to runners come from
[speedrun.com/lba2](https://www.speedrun.com/lba2); the mechanisms and everything marked as
undocumented are read from the source here.

---

## 1. Behaviour switching cancels the current animation

**What runners do.** Tap a behaviour key on landing, or just after a blow lands, to skip the
recovery and act immediately. Runners describe it as resetting the animation to the beginning.

**Why it works.** `SetComportement` ([SOURCES/OBJECT.CPP:790](../SOURCES/OBJECT.CPP)) does three
things on its way out:

```c
ptrobj->FlagAnim = 0;
InitAnim(GEN_ANIM_RIEN, ANIM_REPEAT, NUM_PERSO);
ObjectSetFrame(&(ptrobj->Obj), 0);
```

It clears the animation flags, forces the idle animation, and rewinds to frame 0. The third line
is the one that matters and is the part usually left out of descriptions: it is not that the new
behaviour interrupts the old animation, it is that the animation is *rewound*, so whatever
recovery frames remained are discarded outright.

That is decisive here because hero displacement is not velocity. It is baked into the animation's
keyframe translations and applied by `ObjectSetInterDep` (see
[MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md)). A recovery animation is therefore not a timer
being waited out, it is a sequence of frames each carrying a displacement, and rewinding to idle
frame 0 removes all of them at once.

**Not widely documented:** the same call also swaps the body model and carries a warning from the
original team about doing so.

**`SOURCES/OBJECT.CPP:883`**
```
// ATTENTION: répare un bug lorsqu'on change de cube en NO_BODY
// mais cela peut avoir des effets secondaires, donc méfie Garçon !
```
> "WARNING: fixes a bug when you change cube while in NO_BODY, but this can have side effects, so
> watch yourself, sunshine!"

## 2. The running jump picks a foot, and the animation decides which

**What runners know.** Sporty running jumps are the movement backbone, and some of them are
described as extremely picky.

**Why they are picky, which does not appear to be documented anywhere.** The jump has three
variants and the engine chooses between them by asking which foot is currently planted:

```c
if (ptrobj->WorkFlags & LEFT_JUMP)       Jumping = DO_LEFT_JUMP;
else if (ptrobj->WorkFlags & RIGHT_JUMP) Jumping = DO_RIGHT_JUMP;
else                                     Jumping = DO_NORMAL_JUMP;
```

The comment above it is `// determine pied d'appel`, "work out the take-off foot".

Those two flags are `WorkFlags` bits, and the original names them plainly
([SOURCES/COMMON.H:548](../SOURCES/COMMON.H)):

```
#define	LEFT_JUMP		(1<<15)	// prêt à sauter du pied gauche
#define	RIGHT_JUMP		(1<<16)	// prêt à sauter du pied droit
```
> "ready to jump off the left foot" / "ready to jump off the right foot"

**And nothing in the input code sets them. The animation does.** They are driven by per-frame
animation events, `F_LEFT_JUMP` (37) and `F_RIGHT_JUMP` (38), in the object's data sheet
([SOURCES/FICHE.CPP:858](../SOURCES/FICHE.CPP)):

```c
case F_LEFT_JUMP:
    if (testframe == *ptrc++) {
        ptrobj->WorkFlags &= ~(RIGHT_JUMP);
        ptrobj->WorkFlags |= LEFT_JUMP;
    }
    break;
```

So the walk cycle announces which foot is planted, at a named frame, and the jump reads whatever
the last announcement was. **The timing window for a given jump variant is a frame of the walk
animation, not a window in time.**

Two consequences follow, and the second is the useful one:

- The variant is deterministic given the walk-cycle frame, so it is learnable rather than random.
- **Animation advance is per rendered frame, not per unit of time** (again
  [MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md)), so the walk cycle runs at a rate that depends
  on frame rate. The window therefore moves relative to wall-clock as frame rate changes. This is
  the most likely explanation for jumps being picky on some setups and not others, and it means a
  run's movement tech is not portable across frame rates. See section 6.

There is also a wind-up state. `Jumping = 1024` is set when a jump is being primed from a walk,
and the low bits carry the variant:

**`SOURCES/OBJECT.CPP:4408`**
```
// Seulement si on n'est pas en amorce
// de saut en course
```
> "Only if we are not in the wind-up for a running jump"

## 3. Holding a button and re-pressing it are different inputs

Two paths read input *history* rather than input state, so a hold and a re-press of the same
button produce different results. Neither is documented outside the source.

### Up or Down across a jump

**`SOURCES/OBJECT.CPP:4396`**
```
// skip pour ne pas interrompre un saut
// si le joueur n'a pas relacher Up et Down
// avant de reappuyer dessus
```
> "skip, so as not to interrupt a jump, if the player has not released Up and Down before pressing
> them again"

The test is `Jumping & 1023 AND LastMyJoy & (I_UP | I_DOWN) AND !FlagClimbing`. If the direction
was *already* held on the previous frame the whole block is skipped and the jump survives. Release
and press again mid-jump and `LastMyJoy` no longer carries the bit, so the jump prep is cleared
instead. Keeping the key down is not the same as pressing it again, and only one of the two
preserves the jump.

### Attacking in Aggressive

Holding attack gives the same animation every time. The 1997 team documented it as a bug and
shipped it:

**`SOURCES/OBJECT.CPP:4111`**
```
//	Ici, il y a un bug, le IF qui suit empeche de retirer au hasard une
//	nouvelle anim. Resultat, Twinsen combat toujours avec le meme coup...
```
> "There is a bug here: the IF below stops a new animation being drawn at random. Result: Twinsen
> always fights with the same blow..."

The guard is `if ((LastInput & I_ACTION_M) AND (ptrobj->GenAnim != GEN_ANIM_RIEN)) break;`, sitting
directly in front of a `MyRnd(3)` that would otherwise pick one of three attack animations. So
**releasing and re-pressing attack draws a new animation; holding it does not.** This is the
mechanism underneath the community's habit of re-pressing rather than holding during a fight.

The line above the guard is a note to self from the same session:

**`SOURCES/OBJECT.CPP:4108`**
```
/* essai control direction pendant combat */
```
> "trying out directional control during combat"

It is not a trial any more: `Obj.Beta` is advanced from the turn accumulator on that path, so the
hero can be steered while attacking.

## 4. Simultaneous opposite directions resolve first-listed-wins

Not documented anywhere, and easy to depend on without noticing.

| Pressed together | Result | Where |
|---|---|---|
| Up and Down | Up wins; `if (Input & I_UP) ... else test_down = TRUE` gates Down behind it | [OBJECT.CPP:4440](../SOURCES/OBJECT.CPP) |
| Left and Right | Left wins, both in the turn block and in `ManualRealAngle` | [OBJECT.CPP:3864](../SOURCES/OBJECT.CPP) |

There is no dedicated handling and no neutral result. Whichever branch the `if` reaches first wins,
consistently. This matters on a gamepad, where a stick near a diagonal boundary can set two
opposing bits through the quantiser, and on keyboards where both arrows and keypad are bound to
the same actions.

## 5. Two behaviour keys in the same frame resolve by list order, not by press

Also undocumented. The four behaviour keys are an if/else-if chain
([SOURCES/PERSO.CPP:1441](../SOURCES/PERSO.CPP)):

```c
if (Input & I_NORMAL)        SetComportement(C_NORMAL);
else if (Input & I_SPORTIF)  SetComportement(C_SPORTIF);
else if (Input & I_AGRESSIF) SetComportement(C_AGRESSIF);
else if (Input & I_DISCRET)  SetComportement(C_DISCRET);
```

So Normal beats Sporty beats Aggressive beats Discreet, regardless of which key was pressed first
or which was pressed most recently. A rolled input across two behaviour keys lands on whichever is
earlier in that list.

## 6. Frame rate changes movement tech, not just movement speed

The one section here that is a **claim rather than a measurement**, flagged as such because it is
the most consequential for runs.

Two facts that are measured, in [MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md):

- Walking, running and jump length are animation-driven and applied once per rendered frame, so
  distance covered per unit of game time varies with frame rate, with a sweet spot near 60 fps and
  losses both above and below it. At very high frame rates the slowest movers stop entirely.
- Turning is different. It goes through the `MOVE` accumulator, which is velocity times elapsed
  time with a carried remainder, so it is frame-rate independent.

Three things follow that have not been tested and would be worth a runner's attention:

1. **Arc radius should change with frame rate.** Holding a direction and a turn together walks an
   arc whose turn rate is correct and whose forward distance is not, so the same input carves a
   tighter circle at high frame rates.
2. **Running-jump foot windows move**, per section 2, because the walk cycle advances per frame.
3. **Any strategy tuned at one frame rate may not transfer to another**, which is a different
   claim from the usual "the game runs faster or slower".

If the community has frame-rate rules already, they are likely to be empirical versions of these,
and comparing notes would settle whether the mechanism above matches what runs actually show.

## What is here that is not on the internet

For anyone checking this against community knowledge rather than reading it fresh:

| Known outside | Added here |
|---|---|
| behaviour switch cancels animations | it *rewinds* to frame 0, and displacement lives in the frames, which is why recovery vanishes rather than being skipped |
| Sporty is the movement mode; some jumps are picky | the take-off foot comes from animation keyframe events, so the window is a walk-cycle frame and moves with frame rate |
| re-press rather than hold when attacking | the mechanism is a guard in front of `MyRnd(3)`, documented as a bug in 1997 |
| nothing | holding versus re-pressing a direction changes whether a jump survives |
| nothing | simultaneous opposite directions resolve first-listed-wins |
| nothing | two behaviour keys in one frame resolve by list order |
| the game behaves oddly at unusual frame rates | which parts are animation-driven and which are time-driven, and that the split predicts arcs and jump windows changing shape |

## Could the original behaviour be preserved alongside fixes?

Possible, and cheaper than it looks, because the record is structural rather than a habit somebody
has to keep.

**The delta is always computable.** [2point21/lba2-classic](https://github.com/2point21/lba2-classic)
is frozen: twelve commits, last touched 2021-12-22. This fork's import commit `333929ab` *is* that
tree, so `git diff 333929ab..main` is the complete list of everything this port changed, derivable
at any time by anyone, with no per-commit discipline required. The surface is bounded too: of the
files that existed at the import, **52 original `SOURCES/*.CPP` files have been edited**. That is
the whole territory a compatibility mode would have to reason about.

**But the frozen source is not the retail oracle, and this is the part that catches people.**
The 1997 build used assembly: 280 `.ASM` files are present at the import. The Code Reborn release
translated those to C++, and some translations were wrong. The clearest is #357, where the ASM's
`neg eax` became `radius = ~radius`, two's complement mistaken for bitwise-not:

```c
-        radius = ~radius;
+        radius = -radius; // ASM `neg eax`: two's-complement, not bitwise ~ (#357)
```

So `lba2-classic` faithfully records **what 2point21 released**, not **what retail did**. Where the
two disagree, this fork has the better oracle: 81 of the original `.ASM` files are still in the
tree and the equivalence suite runs them in CI on every push.

**That makes the delta two-directional, which is the useful finding.** Changes since the import
fall into three buckets, not two:

| Bucket | Example | A vanilla mode would want it |
|---|---|---|
| Restores retail, fixing a port regression | the sphere radius above, and the other ASM mistranslations | **on**, because it moves toward retail |
| Diverges from retail deliberately | time-scaling the holomap globe so it stops spinning with frame rate | **off**, this is the compat surface |
| No behavioural effect | reformatting, refactors, new translation units | irrelevant |

A naive "revert to lba2-classic" would therefore be wrong: it would reintroduce bugs retail never
had. The classification is the work, and it is a bounded pass over 52 files against a fixed
reference rather than archaeology across hundreds of commits.

**What stays genuinely hard** is section 6. A Doom-style compatibility level fully determines
behaviour because Doom's simulation is a fixed 35 Hz tic. This game's is not, so vanilla code paths
alone would not reproduce retail outcomes; a faithful mode needs a pinned cadence as well, which is
what `--fixed-dt` already provides for testing. Preserving "the original" here means choosing
between original *code paths*, which is reproducible, and original *outcomes*, which were never a
property of the game alone.

**The limit that used to sit here has moved.** The RNG was a single shared libc `rand()` stream
whose sequence differed by platform (#530), so a recording could not replay on another operating
system however faithful the game logic was. The engine now carries the generator itself, and a
recording crosses between Linux and Windows unchanged. What remains is the arithmetic: `LIB386/3D`
rounds in `long double`, which is 80-bit on x86-64 and 64-bit on ARM, so macOS and Android are not
yet in the set. See [RECORDING.md](RECORDING.md).

None of this argues for building the switch now. It argues that the switch can be built whenever
someone wants it, that the fixtures in [plan/INPUT_PLAN.md](plan/INPUT_PLAN.md) are already the
executable half of the record, and that the classification above is what a future maintainer would
actually be asking for.

## For anyone changing this code

Everything above is behaviour somebody relies on. The input work planned in
[plan/INPUT_PLAN.md](plan/INPUT_PLAN.md) records these as fixtures precisely so that they are
pinned before anything moves. Two of them need care beyond that:

- The Aggressive-attack guard is an original bug. Removing it would look like a fix and would
  change combat for every existing run.
- Anything that presses attack draws from `MyRnd`, which is one shared stream, so a test that
  exercises combat perturbs the RNG for everything after it. Fix the seed and assert on the reset
  counters rather than on which animation was drawn.
