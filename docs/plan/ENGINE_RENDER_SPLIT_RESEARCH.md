# Separating the renderer from the simulation: research

What it would take to give the engine a clock of its own, independent of the renderer, in the
shape Doom uses. Findings only: it commits nothing, plans nothing, and proposes no change.

The premise deserves stating precisely, because half of it already shipped. "The simulation runs
on its own clock, the renderer draws whatever the latest state is" is the fixed-timestep loop that
landed for #358 ([MOVEMENT_FRAMERATE.md](../MOVEMENT_FRAMERATE.md)). What has not shipped is the
other half of Doom's design, which is that the tick is the only loop in the program. This doc
separates the two, measures what is left, and prices each piece.

Line numbers were verified against the working tree at the time of writing. Treat them as "look
here"; the grep commands that produced every count are at the end, so re-run them rather than
trusting a number that has aged.

## What was read

Doom, from a local clone of `id-Software/DOOM`, the `linuxdoom-1.10` tree: `d_main.c` (1171
lines), `d_net.c` (767), `g_game.c` (1690), `m_menu.c` (1893), `i_system.c`, `doomdef.h`. Doom II
is the same engine, so the loop below is the one it runs.

This tree: [SOURCES/PERSO.CPP](../../SOURCES/PERSO.CPP) (3004),
[SOURCES/OBJECT.CPP](../../SOURCES/OBJECT.CPP) (6788),
[LIB386/SYSTEM/TIMER.CPP](../../LIB386/SYSTEM/TIMER.CPP) (343),
[LIB386/SVGA/DIRTYBOX.CPP](../../LIB386/SVGA/DIRTYBOX.CPP),
[LIB386/SYSTEM/EVENTS.CPP](../../LIB386/SYSTEM/EVENTS.CPP),
[SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP), [SOURCES/RECORD.CPP](../../SOURCES/RECORD.CPP),
and the modal loops in GAMEMENU, INVENT, MESSAGE, CONFIG, HOLOPLAN, HOLOGLOB, COMPORTE, CREDITS.

## What Doom does

`D_DoomLoop` (d_main.c:354) is one `while (1)` at d_main.c:369:

```c
I_StartFrame ();
TryRunTics ();                              // will run at least one tic
S_UpdateSounds (players[consoleplayer].mo);
D_Display ();
```

`TryRunTics` (d_net.c:636) reads `I_GetTime()`, which returns 35 Hz tics off `gettimeofday`
(i_system.c:88, `TICRATE` 35 in doomdef.h:122), works out how many tics are due, and runs them:

```c
while (counts--)
    for (i=0 ; i<ticdup ; i++) {
        if (advancedemo) D_DoAdvanceDemo ();
        M_Ticker ();
        G_Ticker ();
        gametic++;
    }
```

Two properties come out of that, and they are worth keeping apart because only one of them is the
famous one.

**The sim runs 0..N times per rendered frame off a clock the renderer does not own.** This is the
property people mean by "fixed timestep". Note the direction Doom chose: `counts` is floored at 1
(d_net.c:676) and the loop below it (d_net.c:714) blocks until a tic is available, so the renderer
never outruns the tick. Vanilla Doom presents at most 35 frames a second, and the clock is the
thing in charge.

**There is exactly one loop in the program.** The menu is `M_Ticker`, called from inside the tick
loop. Intermission, finale and the demo screen are `gamestate` branches in `G_Ticker`
(g_game.c:727) with drawing branches to match in `D_Display`. Nothing blocks, nothing spins, and
nothing else advances the clock. Even the wait loop in `TryRunTics` calls `M_Ticker` before giving
up, under the comment `// don't stay in here forever -- give the menu a chance to work`.

The one exception is the screen wipe, at the tail of `D_Display` (d_main.c:331-344): a `do` loop
that reads `I_GetTime` itself, calls `wipe_ScreenWipe`, `M_Drawer` and `I_FinishUpdate`, and does
not tick the game. Fourteen lines, one of them, in the whole engine.

## Half of it already exists here

The #358 fixed-timestep loop is Doom's first property. Per iteration of `MainLoop`
([PERSO.CPP:491](../../SOURCES/PERSO.CPP#L491)):

| Step | Site | Cadence |
|---|---|---|
| Input poll | [PERSO.CPP:611](../../SOURCES/PERSO.CPP#L611) `MyGetInput` | once per rendered frame |
| Clock advance | [PERSO.CPP:624](../../SOURCES/PERSO.CPP#L624) `ManageTime` | once per rendered frame |
| Step plan | [PERSO.CPP:1576-1624](../../SOURCES/PERSO.CPP#L1576-L1624) `Timer_PlanSimSteps` | once per rendered frame |
| Simulation | [PERSO.CPP:1643](../../SOURCES/PERSO.CPP#L1643) to [1964](../../SOURCES/PERSO.CPP#L1964) | 0 to 8 times, `FixedTimestep` ms each |
| Render | [PERSO.CPP:1978](../../SOURCES/PERSO.CPP#L1978) `AffScene` | once per rendered frame |

`FixedTimestep` defaults to 16 ([GLOBAL.CPP:90](../../SOURCES/GLOBAL.CPP#L90)), the clamp is 8
steps, and the whole thing is byte-identical to the historical path under `--fixed-dt 16`.

So the sim already runs at its own rate, faster or slower than the display, and the render already
draws whatever the last completed step left behind. What is missing is the second property, and
everything below is an inventory of what stands in its way.

## What still ties the two together

### The modal subloops own the clock

A scan for loop bodies containing both a literal `ManageTime()` and a literal present finds 21,
across 11 files:

| Site | Lines | Function |
|---|---|---|
| [AMBIANCE.CPP:738](../../SOURCES/AMBIANCE.CPP#L738) | 26 | `FadePalToPal` |
| [CONFIG.CPP:709](../../SOURCES/CONFIG.CPP#L709) | 237 | `MenuConfig` |
| [CONFIG.CPP:1041](../../SOURCES/CONFIG.CPP#L1041) | 216 | `MenuGamepadConfig` |
| [EXTFUNC.CPP:1859](../../SOURCES/EXTFUNC.CPP#L1859) | 12 | `AffPalette` |
| [GAMEMENU.CPP:1218](../../SOURCES/GAMEMENU.CPP#L1218) | 101 | `InputPlayerName` |
| [GAMEMENU.CPP:1455](../../SOURCES/GAMEMENU.CPP#L1455) | 400+ | `ChoosePlayerName` |
| [GAMEMENU.CPP:3602](../../SOURCES/GAMEMENU.CPP#L3602) | 17 | `ZoomSavedGame` |
| [GAMEMENU.CPP:4415](../../SOURCES/GAMEMENU.CPP#L4415) | 21 | `GameOver` |
| [GAMEMENU.CPP:4683](../../SOURCES/GAMEMENU.CPP#L4683) | 55 | `SlideShow` |
| [HOLOGLOB.CPP:1021](../../SOURCES/HOLOGLOB.CPP#L1021) | 149 | `HoloGlobe` |
| [HOLOPLAN.CPP:799](../../SOURCES/HOLOPLAN.CPP#L799) | 135 | `HoloPlan` |
| [INTEXT.CPP:450](../../SOURCES/INTEXT.CPP#L450) | 52 | `FlyCamera` |
| [INVENT.CPP:902](../../SOURCES/INVENT.CPP#L902) | 18 | `OpenInventory` |
| [INVENT.CPP:1662](../../SOURCES/INVENT.CPP#L1662) | 153 | `DoFoundObj` |
| [INVENT.CPP:1972](../../SOURCES/INVENT.CPP#L1972) | 72 | `RouleauRight` |
| [INVENT.CPP:2074](../../SOURCES/INVENT.CPP#L2074) | 71 | `RouleauLeft` |
| [INVENT.CPP:2249](../../SOURCES/INVENT.CPP#L2249) | 42 | `GereArdoise` |
| [MESSAGE.CPP:2365](../../SOURCES/MESSAGE.CPP#L2365) | 36 | `MyDial` |
| [PERSO.CPP:386](../../SOURCES/PERSO.CPP#L386) | 17 | `GamePaused` |
| [PERSO.CPP:491](../../SOURCES/PERSO.CPP#L491) | 400+ | `MainLoop` |
| [PLAYACF.CPP:553](../../SOURCES/PLAYACF.CPP#L553) | 81 | `PlayAcf` |

21 is a floor, not a total. The scan gives up after 400 lines, so the two entries marked 400+ are
truncated. It also misses any loop whose present happens inside a helper, which is how
`MenuComportement` ([COMPORTE.CPP:530](../../SOURCES/COMPORTE.CPP#L530)) escapes it, and any loop
that pumps through the `ManageSystem()` macro (`ManageEvents(); ManageTime();`,
[TIMER.H:121](../../LIB386/H/SYSTEM/TIMER.H#L121)) rather than calling `ManageTime` directly,
which is how the end credits loop ([CREDITS.CPP:358](../../SOURCES/CREDITS.CPP#L358)) escapes it.

The population behind those loops: 100 `ManageTime()` calls and 25 `ManageSystem()` sites in
`SOURCES` and `LIB386`, and 81 `MyGetInput()` calls across 16 files. Each loop decides its own
clock policy locally, with `LockTimer`/`SaveTimer` pairs bracketing the blocking call at the other
end (61 such sites in PERSO.CPP alone, 23 in GAMEMENU.CPP, 20 in AMBIANCE.CPP). While any of them
is up, `MainLoop` is not running and the simulation is not stepping.

This is the whole gap, and it is the expensive part. Everything else in this section is small by
comparison.

### The game calls the renderer

46 `AffScene` call sites outside its definition, in 9 files: GAMEMENU 9, PERSO 8, MESSAGE 7,
INVENT 6, RES_SWITCH 4, GERELIFE 4, EXTFUNC 4, OBJECT 2, INTEXT 2.

The four in [GERELIFE.CPP](../../SOURCES/GERELIFE.CPP) are the interesting ones: a Life script
opcode reaching for a full-scene redraw mid-script, at
[GERELIFE.CPP:1826](../../SOURCES/GERELIFE.CPP#L1826) and
[1845](../../SOURCES/GERELIFE.CPP#L1845). Doom has no equivalent. Nothing in `P_*` calls `R_*`.

### The renderer writes the world

`AffScene` ([OBJECT.CPP:5356](../../SOURCES/OBJECT.CPP#L5356) to 6255, 900 lines) is not a read of
the world. Its tail does world housekeeping:

- the cube-change autosave, [OBJECT.CPP:6210](../../SOURCES/OBJECT.CPP#L6210);
- palette sync and the screen fade, [6225](../../SOURCES/OBJECT.CPP#L6225);
- the music fade-in and resume state machine, [6238](../../SOURCES/OBJECT.CPP#L6238);
- `LockTimer` at [5452](../../SOURCES/OBJECT.CPP#L5452), released at
  [6192](../../SOURCES/OBJECT.CPP#L6192) and [6197](../../SOURCES/OBJECT.CPP#L6197). The render
  holds the game clock down while it draws.

The sharpest case is in the object pass. When the followed object is fully occluded,
`AffOneObject` calls `CameraCenter(1)` and returns TRUE
([OBJECT.CPP:4968](../../SOURCES/OBJECT.CPP#L4968)); `AffScene` then drops the timer lock,
promotes the frame to `AFF_ALL_FLIP`, and restarts itself from the top via `goto startaffscene`
([OBJECT.CPP:5812](../../SOURCES/OBJECT.CPP#L5812)). The renderer moves the camera and re-runs
itself inside one frame.

Two narrower writes, both already found by the #412 spike
([RENDER_INTERP_PLAN.md](RENDER_INTERP_PLAN.md)): the `OBJ_BACKGROUND` bake into the `Screen`
plate ([OBJECT.CPP:4975](../../SOURCES/OBJECT.CPP#L4975)) and the `WAIT_COORD` extra-origin
write-back ([OBJECT.CPP:4990](../../SOURCES/OBJECT.CPP#L4990)), which reads a point's screen
coordinates back into simulation state after the draw.

### A present is a tick

`BoxBlit` ([DIRTYBOX.CPP:396](../../LIB386/SVGA/DIRTYBOX.CPP#L396)) opens with
`Timer_FixedDtPresent()`, so under the harness clock the act of putting a frame on screen is what
moves game time. It then calls `ManageEvents()`, which is a second pump rather than the only one:
the input poll has its own, `GetInput` to `ManageKeyboard` to `UpdateKeyboardState`
([KEYBOARD.CPP:64](../../LIB386/SYSTEM/KEYBOARD.CPP#L64)), so events are drained twice a frame
from two unrelated places. The pump is untidy; the tick is the problem.

The consequences are documented rather than hypothetical. The console redraws every frame while it
is open, which is a second present per tick, which ran the game clock at double rate for as long
as the console was up. The fix was to mark that present as an overlay
([INPUT.CPP:267](../../SOURCES/INPUT.CPP#L267) calling `Timer_FixedDtOverlayPresent`), which is
the right patch for a design where a present is a tick. See
[TIMING.md](../TIMING.md#an-overlay-is-not-a-frame).

The harness reached for the present as a tick source because the engine has no tick of its own to
hook. The recorder then had to undo it: `Record_ClockHook`
([TIMER.CPP:268](../../LIB386/SYSTEM/TIMER.CPP#L268), called from `ManageTime` at line 273 and
implemented at [RECORD.CPP:2292](../../SOURCES/RECORD.CPP#L2292)) latches one clock reading per
frame ([RECORD.CPP:1390](../../SOURCES/RECORD.CPP#L1390)) so that every `ManageTime` call in a
frame reads the same value. That latch is a per-frame tick, built inside the recorder, for want of
one in the engine.

### Input is sampled per frame, not per tic

Doom builds one `ticcmd_t` per tic. Here `MyGetInput` runs once per rendered frame and the sample
is held across the frame's sub-steps. An edge that lands on a frame where no step runs is never
observed, which is why `Timer_ForceStepIfPending`
([PERSO.CPP:1620](../../SOURCES/PERSO.CPP#L1620)) exists: it promotes a would-skip frame to a
single step when a one-frame signal is pending. Three conditions, three regression tests, two
issues behind it (#358 for the inventory item-use and the scene-flip flag, #407 for the action and
fire edges). A command built per tick removes the class rather than the three instances.

## What each step would take

Ordered by cost. B and C carry no behavioural risk; A was listed here as the third such step and
is not one, which the note under it explains.

| | Work | Cost | Changes behaviour |
|---|---|---|---|
| A | A tick the engine owns | not separable from B | deadlocks as stated |
| B | One pump for the modal loops | medium | no |
| C | Housekeeping out of `AffScene` | medium | no |
| D | One input command per tick | medium to large | fixes bugs |
| E | Gamestate tickers | large | yes |

**A. A tick the engine owns.** One entry point advances the game clock; presents never do. It ends
the present-equals-tick class outright, which is the class the console double-clock came from.

The costing above was written before anyone tried it, and it is wrong on all three counts.
[ENGINE_TICK_POLICY_SURVEY.md](ENGINE_TICK_POLICY_SURVEY.md) surveys the four policies that decide
when to mint, and measures what happens when presents stop minting. **Presents that never mint
deadlock the game**: `OpenInventory` waits for the game clock to move 300 ms with its own
`BoxUpdate` as the only thing that moves it, and the end credits scroll on the same arrangement.
Both run to a timeout where the control exits 0. So step A as stated is not a consolidation, it is
a deadlock, and it is not behaviour-neutral.

The consolidation half is already done: every millisecond the pinned clock hands out goes through
`FixedDtStep` ([TIMER.CPP:184](../../LIB386/SYSTEM/TIMER.CPP#L184)), which #630 finished. The
deletions cannot happen until every clock-terminated loop has a call of its own to advance the
clock with, and giving them one is step B. So the two are not consecutive: **the deletion half of
step A is step B**, and the ordering below has them the wrong way round. The recorder's frame-clock
latch is a third question again: `Record_ClockHook` returns before reading anything while the
pinned clock is armed ([RECORD.CPP:2462](../../SOURCES/RECORD.CPP#L2462)), so it exists for the
host-sampled clock, where an engine tick would give it a better place to latch than the input poll
but would not retire it.

**B. One pump for the modal loops.** Replace the hand-rolled bodies with a shared call that pumps
events, advances the clock by one tick, and presents. No control flow moves, no loop is unrolled,
and every modal ends up agreeing on clock policy instead of deciding it locally 21-plus times over.
This is the step that makes "the engine owns the clock" true even while a menu is up, and it is
the precondition for D and E.

**C. Housekeeping out of `AffScene`.** Autosave, palette and fade, music state, and the timer lock
move into the step or into a post-step call, leaving the render a read of the world. The camera
recentre restart is the hard one and needs its own decision. This is the precondition for anything
that renders more than once per tick, which includes the parked interpolation work.

**D. One input command per tick.** Build the command in the step rather than the frame, and delete
the force-step promotions. The recorder moves with it: it records per poll today, so the format and
the replay path are both in scope. [RECORDING_RESEARCH.md](RECORDING_RESEARCH.md) already sits on
this seam, and its Doom section makes the same point from the other side.

**E. Gamestate tickers.** The modals become states of one loop, each a ticker and a drawer, which
is the actual Doom shape. `ChoosePlayerName` is a single loop over 400 lines, `MenuConfig` and
`MenuGamepadConfig` are 237 and 216, and each is written as a blocking function that returns the
player's choice. Turning those into state machines is a UI-layer rewrite, and it is where most of
the cost of this whole idea sits.

## The one thing that changes behaviour

Doom keeps ticking the world behind the menu. Single-player Doom does not pause when you open the
menu, only when you press pause, and `M_Ticker` runs inside the same tick loop as `P_Ticker`.

This engine does the opposite, deliberately. `SaveTimer`/`RestoreTimer` snapshots the game clock on
entry to a modal and assigns it back on exit, so the elapsed time is discarded and the world is
exactly where it was ([TIMING.md](../TIMING.md#lock-vs-save--they-are-not-the-same)). A dialogue,
an inventory screen or the behaviour menu freezes the game.

That freeze is not incompatible with a ticker model: a state that does not tick the world is a
normal thing for such a loop to have. But the freeze currently comes for free, inherited from
whichever loop happens to be spinning, and under E it has to be re-expressed per screen. That is
a preservation risk in proportion to the number of screens, which is the reason E is last and the
reason it is worth asking whether E is wanted at all, rather than A through C.

## What this is not

A render thread. Roughly 200 globals are shared across the domains
([ARCHITECTURE_GLOBALS.md](../ARCHITECTURE_GLOBALS.md)), the render reads most of the world
directly, and `AffScene` writes to it. Doom did not thread this either. Nothing in this doc
depends on threading and none of A through E is a step toward it.

Nor is it render interpolation. That is #412, already built through phase 2 on `feat/render-interp`
and parked because the residual judder was not perceptible on any tested display
([RENDER_INTERP_PLAN.md](RENDER_INTERP_PLAN.md)). It is the payoff item that C unblocks, not a
prerequisite for anything here.

## Related

- [MOVEMENT_FRAMERATE.md](../MOVEMENT_FRAMERATE.md): the #358 fixed-timestep loop, which is the
  half of this that already shipped, and the clock model sketched at the end of it.
- [TIMING.md](../TIMING.md): the two clocks, `ManageTime`, lock versus save, and the overlay
  present.
- [FIXED_DT_PLAN.md](FIXED_DT_PLAN.md): the harness virtual clock the sim loop reuses.
- [RENDER_INTERP_PLAN.md](RENDER_INTERP_PLAN.md): what to do with the surplus frames once the two
  rates differ.
- [RECORDING_RESEARCH.md](RECORDING_RESEARCH.md): the per-tick input command, from the recorder's
  side.
- [ENGINE_TICK_POLICY_SURVEY.md](ENGINE_TICK_POLICY_SURVEY.md): step A, surveyed and measured.
  Whether the four mint policies collapse, what a present costs at each surface, and the two loops
  that deadlock if presents stop minting.
- [ENGINE_GAME_SEAM.md](../ENGINE_GAME_SEAM.md): the other axis of the same separation, engine
  against game rather than simulation against render.

## Commands

Every count above came from one of these, run from the repository root.

```sh
# ManageTime call sites (100). Two definitions are dropped, and so is TIMERWIN.CPP, which
# has six of them and is not in the LIB386/SYSTEM build list.
grep -rn "ManageTime()" SOURCES LIB386 --include=*.CPP --include=*.C \
  | grep -v "void ManageTime" | grep -v TIMERWIN | wc -l

# ManageSystem sites (25), the macro that pumps events and clock together
grep -rn "ManageSystem()" SOURCES LIB386 --include=*.CPP --include=*.C | wc -l

# AffScene call sites (46, excluding the definition)
grep -rn "AffScene(" SOURCES LIB386 --include=*.CPP --include=*.C | grep -v "void AffScene" | wc -l

# MyGetInput call sites (81 across 16 files)
grep -rln "MyGetInput()" SOURCES LIB386 --include=*.CPP --include=*.C | wc -l

# ManageEvents call sites (12, after dropping the definition and one commented-out block)
grep -rn "ManageEvents()" SOURCES LIB386 --include=*.CPP --include=*.C | grep -v "void ManageEvents" 

# clock lock and save pairs, by file
grep -rn "LockTimer()\|SaveTimer()\|RestoreTimer()\|UnlockTimer()" SOURCES LIB386 \
  --include=*.CPP --include=*.C | awk -F: '{print $1}' | sort | uniq -c | sort -rn
```

The modal loop table came from a scan that walks each `while`, `do` or `for` in `SOURCES` and
`LIB386`, brace-matches up to 400 lines, and keeps the ones containing both a `ManageTime()` and
one of `BoxUpdate`, `BoxBlit`, `AffScene` or `BoxStaticFullflip`. The 400-line cap and the
literal-call requirement are why the result is a floor.
