# Doom 3 input: research

What id's input path does, read from the GPL source, and which of its design choices are worth
having in this engine. Written as an input to the wider input research rather than as a
proposal: it commits nothing and plans nothing.

Doom 3's input system is Quake 3's, five years on. That makes the *differences* the most useful
part of it, because they are id revising their own design under pressures this engine also has:
a sample rate that must not follow the frame rate, one-shot actions that must not be lost, and a
replay that has to be trustworthy. Where a delta is instructive, the Quake 3 version is named.

Every claim about Doom 3 was read out of the source. Every figure about this tree was grepped
against the working tree. Re-run the commands at the end rather than trusting a number that has
aged.

## What was read

Doom 3, `id-Software/DOOM-3` at `a9c49da`, the source as released under the GPL on 22 November
2011, `ENGINE_VERSION "DOOM 1.3.1"`. Read from a local clone of that repository.

`neo/framework/UsercmdGen.{h,cpp}` (170 and 1,112 lines), `neo/framework/KeyInput.{h,cpp}` (221
and 785), `neo/framework/EventLoop.cpp` (274), `neo/framework/Session.cpp` and
`Session_local.h`, `neo/sys/sys_public.h`, `neo/game/Game.h`, `neo/game/Player.cpp`.

Quake 3 Arena, `id-Software/Quake-III-Arena`, for the deltas: `code/client/cl_input.c`,
`code/client/cl_keys.c`, `code/qcommon/common.c`, `code/game/q_shared.h`.

This tree: [SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP) (446),
[SOURCES/INPUT.H](../../SOURCES/INPUT.H),
[LIB386/SYSTEM/INPUT.CPP](../../LIB386/SYSTEM/INPUT.CPP) (59),
[LIB386/SYSTEM/KEYBOARD.CPP](../../LIB386/SYSTEM/KEYBOARD.CPP) (138),
[LIB386/SYSTEM/EVENTS.CPP](../../LIB386/SYSTEM/EVENTS.CPP),
[SOURCES/JOYSTICK.CPP](../../SOURCES/JOYSTICK.CPP) (470), and the input sections of
[SOURCES/PERSO.CPP](../../SOURCES/PERSO.CPP).

## The path, end to end

| Stage | Doom 3 | This engine |
|---|---|---|
| OS to engine | `Sys_GetEvent` returns one `sysEvent_t` | `SDL_PollEvent` loop in `ManageEvents`, fanned out to weak per-subsystem handlers |
| What is kept | the event, queued, and optionally journalled to disk | nothing; the keyboard handler discards key events except to set `LastInputWasKeyboard` |
| Device state | `idKeyInput::PreliminaryKeyEvent` maintains `keys[].down` from the stream | re-sampled whole each poll: `memset(TabKeys, 0, 256)` then `SDL_GetKeyboardState` |
| Who owns input | `idSessionLocal::ProcessEvent`, one chain, each link consuming or passing | whoever is spinning a `MyGetInput()` loop; 81 call sites in 16 files |
| Key to action | `keys[k].usercmdAction` if it is a movement action, else `keys[k].binding` as text | `DefKeys[]` plus `GamepadKeys[]` folded into one flat (key, mask) table, scanned to rebuild `Input` |
| The frame's intent | `usercmd_t`, built by `idUsercmdGen` on a fixed 60 Hz clock, buffered 64 deep | `Input`, `LastInput`, `Key`, `MyKey`, `TabKeys[]`, all global, all live |
| Consumed by | `game->RunFrame( &cmd )`, once per game tic | the object loop, on whichever frames the sim throttle allows |
| Replay | event journal, and `.cdemo` usercmd demos with a per-tic consistency hash | none |

## The design choices

### 1. One fixed sample rate, and frame time removed from the input maths

```c
const int USERCMD_HZ   = 60;
const int USERCMD_MSEC = 1000 / USERCMD_HZ;
```

Quake 3 generated one command per rendered frame and scaled keyboard turning by
`cls.frametime`, the measured length of the last frame. Doom 3 generates one command per fixed
tic and scales by the constant:

```c
speed = idMath::M_MS2SEC * USERCMD_MSEC * in_angleSpeedKey.GetFloat();
```

Measured frame time does not appear in `idUsercmdGenLocal::AdjustAngles` or `JoystickMove` at
all. Fixing the rate did not merely stabilise the input; it deleted a whole class of
frame-rate-dependent arithmetic from the file. Quake 3's `CL_KeyState`, which returned the
fraction of a frame a key was held so that turning stayed frame-rate independent, has no
counterpart in Doom 3 because there is nothing left for it to correct.

**Asynchronous sampling is a cvar, not the architecture.** `UsercmdInterrupt` (async, called on
the timer) and `GetDirectUsercmd` (synchronous, called inline) run the identical five-step
pipeline: `InitCurrent`, `Mouse`, `Keyboard`, `Joystick`, `MakeCurrent`. `RunGameTic` picks
between them on `com_asyncInput`:

```c
if ( com_asyncInput.GetBool() ) {
    cmd = usercmdGen->TicCmd( lastGameTic );
} else {
    cmd = usercmdGen->GetDirectUsercmd();
}
```

The generator does not know or care. That separation is the reusable part: the pipeline is one
function, and which clock drives it is a setting.

### 2. `usercmd_t` is the whole contract, and it is buffered by tic number

Doom 3's grew from Quake 3's six fields to thirteen, but the shape is unchanged: one small
struct, built at exactly one place, that says everything the player asked for this tic.

```c
class usercmd_t {
    int   gameFrame, gameTime, duplicateCount;
    byte  buttons;
    signed char forwardmove, rightmove, upmove;
    short angles[3], mx, my;
    signed char impulse;
    byte  flags;
    int   sequence;
};
```

`UsercmdInterrupt` files each finished command into a ring indexed by tic:

```c
buffered[(com_ticNumber+1) & (MAX_BUFFERED_USERCMD-1)] = cmd;
```

`MAX_BUFFERED_USERCMD` is 64. The consumer asks for a command *by tic number*
(`TicCmd(lastGameTic)`), so falling behind means reading the ones it missed, not skipping them.
Nothing about "which frame is it now" enters the question.

That single struct is why record, replay, injection and networking are one mechanism rather than
four. Anything that can produce a `usercmd_t` can drive the game, and `game->RunFrame` cannot
tell where one came from. `RunGameTic` proves it in eight lines: if a `.cdemo` is open the
command is read from the file, otherwise it is generated, and the call below is the same either
way.

### 3. One-shot actions do not travel as level bits

The impulse mechanism, and the sharpest single idea in the file. An action that happens once
carries a value plus a flag bit that is *toggled* rather than set:

```c
// idUsercmdGenLocal::Key
if ( action >= UB_IMPULSE0 && action <= UB_IMPULSE61 ) {
    cmd.impulse = action - UB_IMPULSE0;
    cmd.flags ^= UCF_IMPULSE_SEQUENCE;
}
```

The consumer fires when the flag *differs* from the one it last saw, then records what it saw:

```c
// idPlayer::Think
if ( ( usercmd.flags & UCF_IMPULSE_SEQUENCE ) != ( oldFlags & UCF_IMPULSE_SEQUENCE ) ) {
    PerformImpulse( usercmd.impulse );
}
oldFlags = usercmd.flags;
```

A pending action survives any number of unobserved tics, and observing it consumes it exactly
once. There is no list of which actions get this treatment; being an impulse *is* the treatment.

Its limit, because it bounds the recommendation below: `impulse` is one value and the flag is
one bit, so two impulses between two observations coalesce, and two that toggle the flag twice
are both lost. It is a one-deep slot, not a queue. Doom 3 gets away with that because the
observer runs every tic and the commands are buffered, so an observation is never skipped in the
first place. Take both halves or neither.

`idUsercmdGenLocal::Key` also opens with `if ( keyState[keyNum] == down ) return;`, commented
"sometimes we get double message". The edge is filtered once, at the source, rather than at each
consumer.

### 4. Bindings are split by kind, and only one kind is text

This is the correction Doom 3 makes to Quake 3, and it matters here.

In Quake 3 every binding went through the command buffer, movement included: pressing a key
bound to `+forward` formatted a string and queued it as console text. Doom 3 keeps that path for
console commands and takes movement off it entirely. `SetBinding` resolves the binding once, at
bind time, into an action id:

```c
keys[keynum].binding = binding;
keys[keynum].usercmdAction = usercmdGen->CommandStringUsercmdData( binding );
```

and `ExecKeyBinding` then refuses to buffer text for it:

```c
// commands that are used by the async thread don't add text
if ( keys[keynum].usercmdAction ) {
    return false;
}
```

`userCmdStrings[]` is the table doing the resolving: `{"_forward", UB_FORWARD}`,
`{"_attack", UB_ATTACK}`, `{"_impulse13", UB_IMPULSE13}`. A name in a config file, an enum in
the engine, resolved once.

So the mature id design is: **a table mapping a key to an action id, plus a text escape hatch
for things that are genuinely commands.** That is the half this engine already has. Its
`DefKeys`-to-`Input` table *is* `usercmdAction`, arrived at independently.

`SetBinding` also opens with `usercmdGen->Clear()`, so rebinding a key cannot leave the old
action stuck down, and closes with `cvarSystem->SetModifiedFlags( CVAR_ARCHIVE )`, so a rebind
marks the config dirty through the same mechanism an archived cvar does. No separate "did the
bindings change" bookkeeping to get wrong.

### 5. Key names have three columns, and the lookups go both ways

```c
keyname_t keynames[] = {
    {"TAB",      K_TAB,      "#str_07018"},
    {"ENTER",    K_ENTER,    "#str_07019"},
    ...
```

A stable name for the config file, the key code, and a **localised display string** for the UI.
`KeyNumToString( keyNum, bool localized )` picks a column. The config gets a name that never
changes with language; the options screen gets one that does.

The reverse lookups exist as first-class API, because a controls screen needs them:
`KeysFromBinding`, `BindingFromKey`, `NumBinds`, `KeyIsBoundTo`, plus
`ArgCompletion_KeyName` so the console can complete key names. `WriteBindings` emits
`unbindall` and then one `bind "SPACE" "_attack"` line per bound key, into a file that is itself
a script the engine can execute, with the backslash case handled explicitly.

### 6. Input is inhibited per subsystem, not by a flag

```c
void idUsercmdGenLocal::InhibitUsercmd( inhibit_t subsystem, bool inhibit ) {
    if ( inhibit ) {
        inhibitCommands |= 1 << subsystem;
    } else {
        inhibitCommands &= ( 0xffffffff ^ ( 1 << subsystem ) );
    }
}
bool idUsercmdGenLocal::Inhibited( void ) { return ( inhibitCommands != 0 ); }
```

Two subsystems use it, `INHIBIT_SESSION` (menu or console) and `INHIBIT_ASYNC`. Each owns one
bit and can only set or clear its own. Nothing has to know whether anyone else is also
suppressing input, and nobody can turn input back on underneath a subsystem that still wants it
off.

`MakeCurrent` respects it by skipping the whole movement pipeline and, importantly, zeroing the
accumulated mouse deltas rather than banking them:

```c
} else {
    mouseDx = 0;
    mouseDy = 0;
}
```

Suppressed input is discarded, not saved up to arrive at once when the menu closes.

### 7. Ownership is a chain, and each link consumes or passes

`idSessionLocal::ProcessEvent` is the whole router. Quake 3 used a `keyCatchers` bitmask;
Doom 3 uses an ordered chain where each link returns true if it took the event:

1. Escape, if no GUI is up, asks the game with `game->HandleESC` and then opens the menu.
2. `console->ProcessEvent`.
3. `guiTest`, if one is being tested.
4. `guiActive`, the menus.
5. If no map is running, the console takes it unconditionally.
6. Otherwise, on a key *down*, `idKeyInput::ExecKeyBinding`.

One function, six links, in one file. A screen does not spin an input loop; it exists in the
chain.

### 8. Two record and replay systems, at two different layers

**The event journal**, inherited from Quake 3 almost unchanged but promoted into a class of its
own (`idEventLoop`, `EventLoop.cpp`). `idEventLoop::GetRealEvent` is the entire mechanism: with
`com_journal 1` it calls `Sys_GetEvent` and writes the struct to disk; with `com_journal 2` it
never calls the system and reads the struct back instead. Nothing else in the engine knows.
Its cost when off is one `if`.

**Command demos**, which are new. `writeCmdDemo` and `playCmdDemo` record and replay at the
`usercmd_t` layer into a `.cdemo` file. These are not video and not a network stream; they are
the input, one command per tic, replayed through the same `RunGameTic` that live play uses.

The two layers answer different questions. The journal replays a *process*, including everything
that arrived through the system boundary. A command demo replays a *session of play*, and is
portable across anything that consumes commands.

### 9. The recording carries its own oracle

This is the best idea in the file for anything that wants gameplay regression testing.

Each recorded command is stored with a hash of the state it produced:

```c
typedef struct {
    usercmd_t cmd;
    int       consistencyHash;
} logCmd_t;
```

`game->RunFrame` returns a `gameReturn_t` whose `consistencyHash` is documented as "used to
check for network game divergence". `RunGameTic` reuses it for playback:

```c
if ( cmdDemoFile ) {
    if ( ret.consistencyHash != logCmd.consistencyHash ) {
        common->Printf( "Consistency failure on logIndex %i\n", logIndex );
        Stop();
        return;
    }
}
```

So a recording is simultaneously the fixture and the assertion, and a failure names the exact
tic at which the replay stopped matching. One number, computed for the network, paying for the
regression suite as a side effect.

The consequence worth extracting: **a replay does not have to be provably faithful to be
useful, if it can tell you when it stopped being faithful.** That is a much weaker precondition
than "enumerate every non-deterministic input first", and it is reached with a hash and a
comparison.

### 10. Fixing the rate let something be deleted

Quake 3's `sysEvent_t` carried `evTime`, and its whole button layer existed to use it:
`kbutton_t` recorded `downtime`, `IN_KeyUp` credited elapsed milliseconds, `CL_KeyState`
returned a sub-frame fraction, and `+attack 12 34567` appended the key and the timestamp to
every button command so that releases could be matched and sub-frame-corrected.

Doom 3's `sysEvent_t` has no `evTime`. None of that layer survives. Once the sample clock is
fixed, per-event timestamps stop earning their place, because "how much of this tic was the key
held" has one answer.

The tempting reading of Quake 3 is that precise timestamps are the fix for
lost and mistimed input. id's own later answer was that a fixed sample rate is the fix, and
timestamps were the workaround for not having one.

## What this engine does today

Measured against the working tree.

**Sampling is level-triggered, unstamped, and tied to the render loop.**
`UpdateKeyboardState` ([KEYBOARD.CPP](../../LIB386/SYSTEM/KEYBOARD.CPP)) clears
`TabKeys[0..255]` and re-reads `SDL_GetKeyboardState` on every call. A press and release between
two polls is not merely mistimed, it never existed. `Key` is the lowest set scancode, so which
key "the" key is among simultaneous presses is an artifact of scancode ordering.

**The event pump already exists and is already fanned out.** `ManageEvents`
([EVENTS.CPP](../../LIB386/SYSTEM/EVENTS.CPP)) runs the `SDL_PollEvent` loop and hands each event
to `HandleEventsTimer`, `HandleEventsMouse`, `HandleEventsKeyboard`, `HandleEventsJoystick`,
`HandleEventsTouch`, `HandleEventsVideo` and `HandleEventsWindow`, several of them weak symbols.
`HandleEventsKeyboard` receives every key event and, by an explicit comment, keeps none of them
beyond setting `LastInputWasKeyboard`. The hard part of an event queue, one pump with a clean
fan-out, is built. What is missing is anything that remembers.

**The key-to-action table is the right shape already.** `InitInput` folds `DefKeys[]` and
`GamepadKeys[]`, `Key1` and `Key2` each, into a flat 128-entry (key, mask) table that `GetInput`
scans to rebuild `Input`. That is Doom 3's `usercmdAction`, and the pad being folded into the
same table rather than OR-ed in afterwards is the same instinct as Doom 3 running keyboard,
mouse and joystick through one `MakeCurrent`.

**What is missing around that table is the naming.** Bindings are positional indices onto raw
scancodes, written to the cfg as `Input7_1=57` and `Gamepad13=1030`. There is no name table, no
round trip, and no localised display column; the key-config screen reads SDL's English scancode
name through `GetKeyScancodeName`. Slots 32 to 35, the four spells, do not reach `Input` at all
and are read by direct `CheckKey(DefKeys[..])` in PERSO.CPP, so the table has a hole in it that
only a comment marks.

**Suppression is a static latch with no owner.** `NoRepeatInput`
([LIB386/SYSTEM/INPUT.CPP](../../LIB386/SYSTEM/INPUT.CPP), 59 lines in total) masks bits already
held, and `ClearNoRepeatInput` resets it wholesale. The wait and clear macros around it are used
at 109 sites. Any of them can clear a suppression another put there.

**Ownership is per-screen, with one hardcoded exception.** 81 `MyGetInput()` call sites across 16
files, concentrated in GAMEMENU (19), PERSO (15), EXTFUNC (10) and INVENT (8). Each modal screen
spins its own loop and clears its own globals. The console is the exception: handled inline
inside `MyGetInput` with a `memset(TabKeys, ...)`, an `Input = 0` and a one-frame suppress flag.
That is a chain link, written once, for one client, by hand.

**Ninety-three sites compare a raw scancode.** `MyKey == K_...` or `Key == K_...` outside INPUT.CPP.
Each is unbindable by definition and blind to the keypad twin its action has in `DefKeys`.

**The throttle is patched with a whitelist.** `Timer_ForceStepIfPending` forces a sim step only
when a named signal is pending, with `I_ACTION_EDGE` the hand-maintained set of qualifying bits.
The set is currently complete, as [INPUT_SIM_PLAN.md](INPUT_SIM_PLAN.md) verified. It is complete
by audit, and stays complete only until the next post-gate edge consumer.

**Injection is two weak symbols.** `ApplyVirtualKeys` and `ApplyHarnessKeys`, called inside
`UpdateKeyboardState` before the scan, so the touch overlay and the control harness both reach
every modal loop. This is the one place the design already matches the id principle: one funnel
that consumers cannot distinguish from a real device.

## The mapping

Ranked by value per cost, against problems that are already open.

### A. Per-subsystem inhibit, replacing the unowned latch

**Take. Cheapest real win here.** `InhibitUsercmd( subsystem, bool )` over a bitmask is about
eight lines, and it converts "115 sites poke a static that nobody owns" into "each subsystem owns
one bit". Doom 3 needed only two subsystems; this engine would want a handful (console, menu,
dialogue, holomap, cutscene).

**Buys.** The class where one screen's input suppression is cleared by another's exit. It also
gives the console's hand-written suppression a general mechanism to be the first user of, instead
of remaining a special case.

**Costs.** Small for the mechanism. Converting call sites is incremental and can stop at any
point, because the old latch and the new mask can coexist.

Take the discard rule with it: Doom 3 zeroes accumulated mouse delta while inhibited rather than
banking it. Suppressed input should be dropped, not delivered late.

### B. The impulse pattern for one-shot actions, and the buffer that makes it safe

**Take, both halves or neither.** One-shot actions stop travelling as level bits, so no consumer
can miss one by sampling at the wrong moment, and `I_ACTION_EDGE` stops being a list that has to
be maintained.

**Buys.** The open half of the throttle-drop class, structurally rather than by audit. Weapon
select, behaviour select, holomap, pause and item use stop depending on where in `MainLoop` they
are read.

**Costs.** Small in code, larger in review: each converted action's consumer changes from "is the
bit set" to "has the sequence changed". Convert one and measure before converting a second.

The warning from section 3 is the whole recommendation: the one-deep slot is only safe because
commands are buffered and every tic is observed. This engine's sub-step loop can skip several
frames at once, so importing the toggle without something that guarantees observation would
substitute a subtler bug for a visible one. If only one half is affordable, take a counter rather
than a toggle bit.

### C. Named bindings, with a localised display column and reverse lookups

**Take.** A `keynames[]`-equivalent with three columns and working `StringToKeyNum` /
`KeyNumToString( n, localized )`, plus `KeysFromBinding` and `BindingFromKey`.

**Buys.** Four things at once, which is why it ranks high despite being the least exciting item.
The cfg becomes readable and diffable, which matters more here than it did for id given this
game's history of silent cfg misbehaviour. The key-config screen gets localised key names instead
of SDL's English ones, in an engine that already localises its menu labels. The reverse lookups
are exactly what a controls screen and a profile system need, and are currently done by indexing
`DefKeys` positionally. And a named binding is the unit an input profile would be expressed in.

**Costs.** Medium, and it needs a migration path: existing cfgs hold numbers. `unbindall` as a
written header is id's pattern for making a config self-sufficient rather than a patch on
defaults. The 90 raw scancode comparisons do not have to move for this to be worth doing, but
they cap its value until they do.

### D. A chain of responsibility for input ownership, diagnostic first

**Take the idea, not the refactor.** Replacing 81 `MyGetInput()` loops is not a proportionate
move and should not be proposed as one.

What is proportionate: a single "who owns input" value that screens set on entry and clear on
exit, observed only at first, with nothing routed through it. It turns the modal-bypass survey
and the missing-consume survey from findings that need re-deriving into a table that can be
asserted on.

Doom 3's escape rule is worth importing early and cheaply on its own: escape is handled at the
top of the chain, and the *game* is asked first (`game->HandleESC`) before the menu opens. One
rule, and it is the shape of fix that a whole class of modal-escape bug wants.

### E. Command demos with a consistency hash

**Take this as the replay design.** It is the answer
[INPUT_REPLAY_RESEARCH.md](INPUT_REPLAY_RESEARCH.md) went looking for and did not find in the
retail data, and it is better than the journal for this engine's purposes.

**Buys.** A gameplay regression net whose fixtures are recorded play, and which reports the exact
tic where behaviour changed rather than a pass or fail at the end. The hash also does double duty
as a determinism probe for the work already underway there.

**Why it outranks the journal here.** A journal is only worth what its boundary is complete, and
this boundary is wider than id's: dt and the direct clock reads documented in
[TIMING.md](../TIMING.md) sit outside any event queue, and dt is the dominant determinism lever.
Journalling keys alone would produce recordings that do not replay, which is worse than none
because it looks like it works. The consistency hash sidesteps that precondition rather than
satisfying it: it does not make replay faithful, it makes unfaithfulness *visible and located*.
That is a far cheaper thing to build and a far more useful thing to have first.

**Costs.** Needs a stable per-tic hash of gameplay state, which does not exist yet and is the
real work in this item. It also needs the command struct in F, or something standing in for it.
`--fixed-dt` and the control harness already supply the rest.

### F. One frame-intent struct

**The destination, not a task.** A, B and E all push toward the same shape: one small struct
saying what the player asked for this tic, built at one place, consumed by gameplay that cannot
tell whether it came from a keyboard, a pad, the touch overlay, the harness or a file. That one
decision is why record, replay, injection and networking are one mechanism in Doom 3.

Worth writing down as where the items above are heading, so they land in a compatible direction.
Not worth proposing as a change on its own, and not a justification for one.

## What not to take

**The asynchronous input thread, as a thread.** But note the finding in section 1: it is a cvar.
`GetDirectUsercmd` runs the same pipeline synchronously, and `RunGameTic` chooses. Take the
pipeline as one function called from the main loop; the async option stays available and costs
nothing to leave unbuilt. A real async sampler would fight `--fixed-dt` and every fixture built
on it.

**Prediction, reconciliation, snapshots, `delta_angles`.** All of it exists to survive a network.
There is no server here. Nothing to buy.

**Absolute view angles in the command.** In Doom 3 the player's angles are the aim, and
`usercmd_t.angles` is authoritative. Here the hero turns and the camera follows, and the camera's
rules are established by measurement in [CAMERA.md](../CAMERA.md). Adopting the id angle model
would rewrite that contract for no gain.

**Bindings dispatched as text for movement.** Quake 3 did this; Doom 3 explicitly stopped, and
`ExecKeyBinding` returning false for a `usercmdAction` is the code that stopped it. This engine
already has the table. Do not regress toward the text path in the name of matching id.

**Mouse acceleration and filtering.** The mouse camera already has its own smoothing and
sensitivity tunables in the cfg, documented in [CONTROLLER.md](../CONTROLLER.md). Nothing to
import.

## Open questions

1. **How deep does the sub-step skip actually go?** Item B's choice between a toggle bit and a
   counter depends on the maximum number of consecutive frames the sub-step loop can skip in
   practice. Measurable from the existing harness, not yet measured.

2. **What would a consistency hash of this game's state cover?** Item E is blocked on this and
   little else. Hero position and animation are the obvious core; whether actors, RNG draw count
   and scene state belong in it decides whether the hash catches the regressions worth catching
   or trips on noise.

3. **Is the spell-slot hole in the binding table deliberate?** Slots 32 to 35 bypass `Input` and
   are read through `CheckKey` directly. Whether that was a 1997 constraint or a port-era
   shortcut decides whether item C can close it or has to preserve it.

4. **Does the level-sampled path lose anything a player can feel today?** The whitelist audit says
   current exposure is narrow, so item B is justified by fragility rather than by a measured drop.
   If a measured drop is wanted instead, it would need a fast-press fixture at several throttle
   settings, and it does not exist.

## Reproduce

```bash
# This tree, all figures quoted above. Patterns are CPP-only and spacing-tolerant;
# widening them to SOURCES/ as a whole also matches the macro and extern declarations.
grep -rn 'MyGetInput()' --include=*.CPP SOURCES | wc -l                          # 81
grep -rn 'MyGetInput()' --include=*.CPP SOURCES | cut -d: -f1 | sort | uniq -c   # per file
grep -rnE '(MyKey|Key) *== *K_' --include=*.CPP SOURCES | grep -v INPUT.CPP | wc -l   # 93
grep -rn 'ClearNoRepeatInput\|ClearWaitNoInput\|InitWaitNoInput\|InitWaitNoKey' \
     --include=*.CPP SOURCES | wc -l                                             # 109
grep -rn 'Input *& *I_' --include=*.CPP --include=*.H SOURCES LIB386 | wc -l     # 154
grep -rn 'CheckKey(' --include=*.CPP SOURCES | wc -l                             # 21

# Doom 3, from a clone of id-Software/DOOM-3 at a9c49da; paths below are relative to it
neo/framework/UsercmdGen.h        # USERCMD_HZ, usercmd_t, UCF_IMPULSE_SEQUENCE
neo/framework/UsercmdGen.cpp      # MakeCurrent, UsercmdInterrupt, GetDirectUsercmd,
                                  # InhibitUsercmd, userCmdStrings[], Key()
neo/framework/KeyInput.cpp        # keynames[], SetBinding, ExecKeyBinding, WriteBindings
neo/framework/EventLoop.cpp       # GetRealEvent (the journal), ProcessEvent, RunEventLoop
neo/framework/Session.cpp         # ProcessEvent (the chain), RunGameTic, WriteCmdDemo
neo/framework/Session_local.h     # logCmd_t
neo/game/Game.h                   # gameReturn_t.consistencyHash
neo/game/Player.cpp               # the UCF_IMPULSE_SEQUENCE consumer

# Quake 3, for the deltas quoted in sections 4 and 10
B=https://raw.githubusercontent.com/id-Software/Quake-III-Arena/master
curl -sfL $B/code/client/cl_input.c   # kbutton_t, CL_KeyState, CL_CreateCmd
curl -sfL $B/code/client/cl_keys.c    # keyCatchers, the +cmd key time protocol
curl -sfL $B/code/game/q_shared.h     # the six-field usercmd_t
```
