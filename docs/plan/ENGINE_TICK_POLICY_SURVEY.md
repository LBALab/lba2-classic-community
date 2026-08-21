# Can the four mint policies collapse into one? A survey

Step A of the ladder in [ENGINE_RENDER_SPLIT_RESEARCH.md](ENGINE_RENDER_SPLIT_RESEARCH.md) is
"a tick the engine owns". That doc prices it as small, behaviour-neutral, and mostly a
consolidation. The consolidation already happened: every millisecond the pinned clock hands out
goes through `FixedDtStep` ([TIMER.CPP:184](../../LIB386/SYSTEM/TIMER.CPP#L184)), which #630
finished. What did not happen is one place where the *decision* to hand one out is made, and there
are four different answers to that question.

This survey asks whether the four can become one, and answers it with measurements rather than a
reading. The short answer is yes, the collapse is real, and it cannot happen in
[TIMER.CPP](../../LIB386/SYSTEM/TIMER.CPP): it happens in the loop bodies, which is step B. The
long answer is below, along with the counterexample that kills step A as the ladder doc states it,
and a defect class the survey turned up on the way.

Line numbers were read from the working tree at `51fe3890`. The commands and the instrument that
produced every number are at the end; re-run them rather than trusting a figure that has aged.

## The four policies

| Function | Where | Says |
|---|---|---|
| `Timer_FixedDtAdvance` | [TIMER.CPP:204](../../LIB386/SYSTEM/TIMER.CPP#L204) | the main loop iterated |
| `Timer_FixedDtPresent` | [TIMER.CPP:219](../../LIB386/SYSTEM/TIMER.CPP#L219) | a frame reached the screen |
| `Timer_FixedDtPump` | [TIMER.CPP:253](../../LIB386/SYSTEM/TIMER.CPP#L253) | a wait loop iterated without drawing |
| `Timer_FixedDtOverlayPresent` | [TIMER.CPP:217](../../LIB386/SYSTEM/TIMER.CPP#L217) | the next present is not a frame |

Only the first is triggered by a loop iteration. The other three are about a *present*: two of them
say a present is a tick, and the third takes it back for one caller.

## The inventory

Direct callers of the four, complete:

| Policy | Call sites |
|---|---|
| Advance | [CONTROL.CPP:1132](../../SOURCES/CONTROL.CPP#L1132) (recorder-armed), [:1150](../../SOURCES/CONTROL.CPP#L1150) (`--fixed-dt`) |
| Present | [DIRTYBOX.CPP:403](../../LIB386/SVGA/DIRTYBOX.CPP#L403), inside `BoxBlit` |
| Overlay | [INPUT.CPP:267](../../SOURCES/INPUT.CPP#L267), the console |
| Pump | [AMBIANCE.CPP:549](../../SOURCES/AMBIANCE.CPP#L549), [:577](../../SOURCES/AMBIANCE.CPP#L577), [:602](../../SOURCES/AMBIANCE.CPP#L602), [:627](../../SOURCES/AMBIANCE.CPP#L627), [:659](../../SOURCES/AMBIANCE.CPP#L659), [:702](../../SOURCES/AMBIANCE.CPP#L702), [:741](../../SOURCES/AMBIANCE.CPP#L741); [MUSIC.CPP:439](../../SOURCES/MUSIC.CPP#L439), [:496](../../SOURCES/MUSIC.CPP#L496); [RES_SWITCH.CPP:720](../../SOURCES/RES_SWITCH.CPP#L720) |

Thirteen sites, which looks small. It is not, because `Timer_FixedDtPresent` sits inside `BoxBlit`
and `BoxBlit` is the only present path the engine has: `LockVideoSurface` and `UnlockVideoSurface`
have no caller outside [DIRTYBOX.CPP](../../LIB386/SVGA/DIRTYBOX.CPP), and the actual upload runs
from [SDL.CPP:151](../../LIB386/SVGA/SDL.CPP#L151) when the surface unlocks. So the real count is
the number of places that ask for a present:

```
grep -rn "BoxUpdate()\|BoxBlit()\|BoxStaticFullflip()" --include=*.CPP --include=*.C \
  SOURCES LIB386 | grep -v "^LIB386/SVGA/DIRTYBOX.CPP" | wc -l      # 142
```

**142 call sites have a clock policy applied to them, and exactly one of them has ever asked for
one.** That one is the console. The other 141 mint or do not mint according to where they happen to
fall in a tick.

That is the finding, restated: the funnel gives one place where virtual time is created. The
decision to create it is spread over 142 sites, none of which is written as a decision.

## What a present actually costs, measured

An instrumented build counts mints and attributes each present to the site that asked for it
(`__builtin_return_address` captured through `BoxBlit`, `BoxUpdate` and `BoxStaticFullflip`,
symbolised with `addr2line`, ASLR off). Runs are `--headless --fixed-dt 16 --load`, so they are
deterministic and one run is the whole distribution.

Mints inside a single main-loop tick, by surface:

| Surface | Mints in one tick | Reached by |
|---|---|---|
| idle, no modal | 1 | nothing |
| scene change | 15 | `cube 154` |
| config menu | 61 | `ui config` |
| main menu, options, display, dialogue, found object | 63 | `ui menu-main` and siblings |
| inventory | 64 | `ui inventory` |
| holomap, holoplan | 83 | `ui holomap`, `ui holoplan 0` |
| end credits | 7158 | `credits` |

The idle row is the one to notice first: 601 ticks, 601 mints, 600 free presents, nothing extra.
Under the main loop alone the arrangement is exact. Everything above it is a modal.

### Where a modal's mints come from

The inventory decomposes, and the decomposition is the point:

| Site | Mints | What it is |
|---|---|---|
| [INVENT.CPP:918](../../SOURCES/INVENT.CPP#L918) | 20 | `OpenInventory`, the box-opening animation, one present per iteration |
| [INVENT.CPP:852](../../SOURCES/INVENT.CPP#L852) | 40 | `DrawOneInventory`, of which 35 are one pass of `DrawInventoryScreen` |
| elsewhere | 4 | |

[`DrawInventoryScreen`](../../SOURCES/INVENT.CPP#L872) is a `for n < MAX_BOX_INVENTORY` over 35
inventory slots, each ending in `BoxUpdate`. It is not a loop iteration, it is not a wait, and it
is not a frame the simulation asked for. It is one draw of a static grid, and it mints 35 steps of
simulation time, 560 ms.

The nine poll-driven modals do the opposite. `DoGameMenu`
([GAMEMENU.CPP:2575](../../SOURCES/GAMEMENU.CPP#L2575)), `MenuConfig`
([CONFIG.CPP:677](../../SOURCES/CONFIG.CPP#L677)), `SpeakAnimation`
([MESSAGE.CPP:2022](../../SOURCES/MESSAGE.CPP#L2022)), `DoFoundObj`
([INVENT.CPP:1456](../../SOURCES/INVENT.CPP#L1456)), `HoloGlobe`
([HOLOGLOB.CPP:993](../../SOURCES/HOLOGLOB.CPP#L993)) and `HoloPlan`
([HOLOPLAN.CPP:776](../../SOURCES/HOLOPLAN.CPP#L776)) each present about once per iteration and
exit on a keypress, so for them a present really is a tick.

So the heuristic is not uniformly wrong. It is **right at every poll-driven modal and wrong at
exactly two shapes**:

1. **A draw loop that presents per drawn element.** `DrawInventoryScreen`, 35 presents in one pass.
2. **A wait loop that pumps and presents on the same iteration.** Every palette fade in
   [AMBIANCE.CPP](../../SOURCES/AMBIANCE.CPP): the loop calls `Timer_FixedDtPump`, then `FadePal`
   ends in `BoxBlit` ([AMBIANCE.CPP:534](../../SOURCES/AMBIANCE.CPP#L534)), so both steppers fire
   and the iteration costs 32 ms rather than 16. Measured on a `cube 154` transition: seven pumps
   and seven presents, 14 steps for a `FADE_DELAY` of 200 ms, and seven ramp frames where 16 ms an
   iteration would give thirteen.

A range cannot be searched for. Those two shapes can.

### What it costs a player

Every recording arms pacing ([RECORD.CPP:1321](../../SOURCES/RECORD.CPP#L1321)), so during a
recording each minted step is slept out in real time. Clean build of `main`, 300 ticks, N=3:

| Run | Wall time (ms) | Delta | Predicted from mint count |
|---|---|---|---|
| `--record`, no modal | 4957 / 4955 / 4962 | | |
| `--record` + inventory | 5987 / 5903 / 5958 | ~991 | 63 x 16 = 1008 |
| `--record` + scene change | 5192 / 5233 / 5193 | ~235 | 14 x 16 = 224 |

A player who opens the inventory while recording pays about a second of wall time for it. Roughly
320 ms of that is the box-opening animation, which is timed on the clock and legitimately costs
what it mints. The rest is the grid draw sleeping between slots: half a second in which nothing on
screen changes.

The mint count and the wall-clock delta agree to within 2%, which is worth more than either figure
alone. Two instruments that do not share a mechanism land on the same number.

## The verdict

**Yes, the four collapse into one.** One call, made once per iteration by every loop that iterates,
whoever's loop it is. `Timer_FixedDtPresent`, `Timer_FixedDtOverlayPresent` and the
`FixedDtSkipPresent` flag then disappear together, because the only reason any of the three exists
is that presents mint: the free-present flag exists to stop the tick's own render paying twice, and
the overlay flag exists to stop a console redraw paying at all. Neither has anything to say once a
present is just a present. `Timer_FixedDtPump` and `Timer_FixedDtAdvance` become the same call.
What survives is `FixedDtTicking`, the gate that keeps boot presents from moving the clock, and
that is shared state rather than a policy.

**And it cannot happen in TIMER.CPP.** No site in the engine except `MainLoop` currently knows
where an iteration boundary is. 29 loops contain both a present and a read of
`TimerRefHR`/`TimerSystemHR`, and one of the 29 calls `Timer_FixedDtPump`
([AMBIANCE.CPP:738](../../SOURCES/AMBIANCE.CPP#L738)); the pump calls below raise that to three.
The rest get their clock from whatever they happen to draw. Collapsing the policies means adding a call to those loop bodies, which is
step B. So the deletion half of step A is not blocked by step B; it *is* step B.

**Until then the minimum stable set is four.** Not three: drop `Timer_FixedDtOverlayPresent` and
the console double-clock comes back. Not two: drop `Timer_FixedDtPresent` and the game deadlocks,
which is the next section.

### One constraint on the collapse

A collapsed mint must not also collapse *a step of clock was minted* with *a simulation tick
happened*. `Record_TickHook` is called from `Control_TickHook`
([CONTROL.CPP:1122](../../SOURCES/CONTROL.CPP#L1122)), which runs once per `MainLoop` iteration
([PERSO.CPP:583](../../SOURCES/PERSO.CPP#L583)), so the recorder's tick number and its per-tick
state digest are indexed on main-loop iterations specifically. If step B makes every modal
iteration a tick, the digest's index changes meaning and every recording's tick numbering moves at
once. Two concepts, one of which the recorder counts and hashes.

## The counterexample

The ladder doc states step A as "One entry point advances the game clock; presents never do."
Built that way, the game hangs.

The arm is one branch in `Timer_FixedDtPresent` that skips `FixedDtStep`, gated on an environment
variable so the arm and the control are the same binary and the same command line. Eleven surfaces
driven, each twice:

| Surface | Presents mint | Presents do not mint |
|---|---|---|
| inventory | 0 | **124** |
| end credits | 0 | **124** |
| holomap, holoplan, menu-main, menu-options, display, config, slideshow, dialogue, found object | 0 | 0 |

Two hangs of eleven. Both are loops whose exit condition is the game clock and whose only clock
source is their own present:

- [INVENT.CPP:900-919](../../SOURCES/INVENT.CPP#L900), `OpenInventory`. The box grows for
  `15 * 20` ms of `TimerRefHR`, `BoxUpdate` is the only thing in the body that moves it, and there
  is no input poll and no pump. Take the mint away and `x1 != INV_END_X` can never become false.
- [CREDITS.CPP:610](../../SOURCES/CREDITS.CPP#L610), inside `GamePlayCredits`
  ([CREDITS.CPP:93](../../SOURCES/CREDITS.CPP#L93)). The scroll position is
  `ModeDesiredY - (TimerRefHR - scrolltimer) / 20`, and the loop ends when the scroll reaches the
  `.` terminator line ([CREDITS.CPP:626](../../SOURCES/CREDITS.CPP#L626)). It polls, but the poll
  cannot end it; the clock does, and `BoxUpdate` is the clock.

The other nine survive because they exit on a keypress. So the honest bound is two of eleven, not
"the modals hang", and the reason the nine are safe is the same reason the heuristic looks
reasonable at them.

### The control has to be unset, not empty

The arm is gated on `getenv("LBA2_NO_PRESENT_TICK") ? 1 : 0`, which reads an empty-but-set variable
as on. A control that sets the variable to the empty string is a second arm, and the matrix then
comes back with every surface hanging on both sides, which reads as a far larger finding than the
real one. The control has to unset it: `env -u LBA2_NO_PRESENT_TICK`. A control that cannot be
shown to differ from its arm is not a control; see [BUG_HUNTING.md](../BUG_HUNTING.md), "Oracle
discipline".

## A wait with no clock at all

Looking for the counterexample's shape turned up a stricter one: a loop that waits on the game
clock and does not present, pump or delay. Under a pinned clock such a loop has no clock source at
all, so it cannot terminate.

The argument is complete rather than suggestive. `ManageTime` reads `FixedDtNow`
([TIMER.CPP:371](../../LIB386/SYSTEM/TIMER.CPP#L371)); `FixedDtNow` moves only in `FixedDtStep`;
`FixedDtStep` is called only from the three minting policies; none of the three is reachable from a
body whose statements are `ManageTime()` and an input poll. There is no path.

Seven such loops are reachable while the clock is pinned:

| Site | Function | Reached by |
|---|---|---|
| [GAMEMENU.CPP:3715](../../SOURCES/GAMEMENU.CPP#L3715) | `ShowSaveConfirmation` | saving from the menu |
| [GAMEMENU.CPP:4466](../../SOURCES/GAMEMENU.CPP#L4466) | `GameOver` | dying |
| [GAMEMENU.CPP:4739](../../SOURCES/GAMEMENU.CPP#L4739) | `SlideShow` | the end-of-game slides |
| [GAMEMENU.CPP:4973](../../SOURCES/GAMEMENU.CPP#L4973) | `ShowLogo` | the `slide` console verb |
| [INVENT.CPP:2261](../../SOURCES/INVENT.CPP#L2261) | `GereArdoise` | flipping a slate page left |
| [INVENT.CPP:2282](../../SOURCES/INVENT.CPP#L2282) | `GereArdoise` | flipping a slate page right |
| [GAMEMENU.CPP:5104](../../SOURCES/GAMEMENU.CPP#L5104) | `DemoLogo` | a demo-SKU save change |

`DemoLogo` is the odd one: it is `#ifdef DEMO`, so it is compiled only into the demo SKU, but there
it is called from `MainLoop` ([PERSO.CPP:528](../../SOURCES/PERSO.CPP#L528)) and is as mid-session
as the rest.

The same family in `AdelineLogo`, `BumperLogo` and `DemoBumper` is boot-only, guarded
by `!Control_IsActive()` ([PERSO.CPP:2689](../../SOURCES/PERSO.CPP#L2689)), and the pinned clock is
armed after it at [PERSO.CPP:2970](../../SOURCES/PERSO.CPP#L2970), so those cannot be reached with
the clock pinned.

One of the seven is reachable with an existing driver, and the A/B is clean:

| Run | Exit |
|---|---|
| `--exec-at 60 "slide activision" --tick 300 --exit` | 0 |
| the same with `--fixed-dt 16` | **124** |

The other six hold by construction only. `GereArdoise` cannot be entered from a script: `useitem`
sets `InventoryAction`, but the dispatch at [PERSO.CPP:1203](../../SOURCES/PERSO.CPP#L1203) runs
only after `MenuInventory` returns from an `I_INVENTORY` press, and the page flip additionally
needs `NbArdoise >= 2`, which no console verb sets. `GameOver` needs the hero dead and
`ShowSaveConfirmation` needs a save driven through the menu, neither of which any driver in the
tree does. `DemoLogo` needs the demo SKU and its own data. So: mechanism proven, surfaces
unreachable, and said rather than counted as six more reproductions.

This is not the present-as-tick class. It is the class #635 closed for the fade loops, arriving
from the other side: `Record_WaitHook` is reached from `Timer_FixedDtPump`
([TIMER.CPP:254](../../LIB386/SYSTEM/TIMER.CPP#L254)), so a loop with no pump call gets neither the
pinned-clock step nor the recorder's wait hook. #635 added pump calls to the ten loops that needed
them; these seven were not among them because they do not fade.

[TIMING.md](../TIMING.md) already carries the rule they break: "only pump `ManageTime` inside loops
that need the game clock to actually move, and call `Timer_FixedDtPump`/`Timer_FixedDtPresent`
alongside if the loop should also terminate under fixed-dt." The rule postdates the loops.

### The pump a polling wait needs is not the pump a silent one needs

Five of the seven poll input every iteration, and for those the wait hook is not merely unnecessary
but harmful. The hook exists because a loop that never polls gets no new reading from the clock
hook, there being no poll to take one at; a loop that polls gets one per iteration already. Minting
a second step there ends the wait early, and `Record_WaitHook` sleeps out the real time for every
step it mints ([RECORD.CPP:2713](../../SOURCES/RECORD.CPP#L2713)), in loops that run thousands of
iterations a second on a host clock.

Measured on a loose-clock recording that crosses `ShowLogo`, 534,570 polls over 258 ticks, replayed
three ways:

| Binary | Result |
|---|---|
| before any of this | clean in 8 s: 301 ticks checked, no mismatch, 0 ms drift |
| with `Timer_FixedDtPump` in the polling waits | had not reached its first progress line after 60 s |
| with `Timer_FixedDtPumpPolled` in them | clean in 8 s, identical numbers to the first row |

So the two are split. `Timer_FixedDtPump` mints and drives the wait hook, and is for a wait that
does not poll: the two `GereArdoise` loops here, and the ten loops #635 fixed.
`Timer_FixedDtPumpPolled` ([TIMER.CPP:246](../../LIB386/SYSTEM/TIMER.CPP#L246)) mints only, and is
for a wait that does. Outside `--fixed-dt` the polled form is genuinely inert; the unpolled one is
inert only when nothing is recording.

### What the scan counts as a clock source, and what it should

The scan treats a present anywhere in a loop body as a clock source. A present under a condition is
not one. `GameOver`'s zoom loop ([GAMEMENU.CPP:4423](../../SOURCES/GAMEMENU.CPP#L4423)) is the
instance: its only `BoxUpdate` sits inside `if (BodyDisplay(...))` and a clip test, so with the body
fully clipped nothing presents, the deadline never arrives, and it is the same defect as the wait
thirty lines below it that the scan did catch. It now mints its own step. Every other loop the scan
cleared on the strength of a present was re-read for the same shape and none has it, but the scan
cannot be trusted to find the next one.

## A modal recording does not replay, and the cause is not the clock

Whatever step B does to the modal loops will move their clock sequence, so it is worth knowing
what that sequence reproduces today. It does not, and the reason has nothing to do with which
policy minted what.

Record 300 ticks with a modal opened at tick 60 by a console verb, then replay. The fade path is
clean and every modal is not:

| Session | Replay |
|---|---|
| quiet, no modal | 301 ticks checked, no mismatch, 0 ms drift |
| `cube 154` (a scene change, so a fade) | 301 ticks checked, no mismatch, 0 ms drift |
| `ui inventory` | first hash mismatch tick 61, 16 ms drift at tick 61 |
| `ui dialog 1` | first hash mismatch tick 61, 16 ms drift at tick 61 |
| `ui menu-main` | first hash mismatch tick 61, 16 ms drift at tick 61 |
| `ui found-object 0` | first hash mismatch tick 61, 16 ms drift at tick 61 |
| `ui holomap` | first hash mismatch tick 61, 0 ms drift |
| `slide activision` | first hash mismatch tick 61, 16 ms drift at tick 61 |

Tick 61 is the first digest after the modal in every case and the numbers repeat exactly, so this
is deterministic. Telemetry names the field: `sim.carry` 4268109 recorded against 4268125 replayed,
which is `LastSimRefHR` exactly one 16 ms sub-step apart, with actor `LastTimer`/`NextTimer` and
then positions downstream of it.

**The modal runs in a different tick on the two sides.** Tracing every call into the three policies,
with the tick number the advance is counting, the `ui inventory` modal's 64 presents land at tick 60
while recording and at tick 59 while replaying. `cube 154`'s fade lands at tick 61 on both.

That is the whole of it, and the fade is what proves it: `cube` sets `NewCube` and its work happens
at the top of the next `MainLoop` iteration ([PERSO.CPP:523](../../SOURCES/PERSO.CPP#L523)), so a
command that arrives out of position is re-synchronised to a tick boundary before it does anything.
A verb that does its work inline is not.

**The two positions are inside one function, and they are one minted step apart rather than one
tick.** The reading above is a tick attribution, and the thing being attributed is what moved.
Instrumenting both ends to print the tick counter and the clock at the moment the command runs, with
every tick-hook entry as a per-run baseline, puts them on the same tick:

| | recording | replay |
|---|---|---|
| tick-hook entry, tick 60 | clock 1010 | clock 1010 |
| the command runs | clock 1026 | clock 1010 |
| tick-hook entry, tick 61 | clock 2034, `sim.carry` 4268109 | clock 2018, `sim.carry` 4268125 |

Both run it with the recorder's tick counter at 61 and both from `Control_TickHook`
([PERSO.CPP:583](../../SOURCES/PERSO.CPP#L583)). The recording runs it from `control_run_exec_at`,
which is below `Timer_FixedDtAdvance` ([CONTROL.CPP:1150](../../SOURCES/CONTROL.CPP#L1150)); the
replay ran it from `Record_TickHook`, which is that function's first statement, above the advance.
Running above the advance is exactly what puts the modal's presents under the previous tick number
in the trace above, so the two readings are one fact from two sides. Absolute clock values compare
only within a pair: `Timer_EnableFixedDt` seeds `FixedDtNow` from `TimerSystemHR`
([TIMER.CPP:111](../../LIB386/SYSTEM/TIMER.CPP#L111)), so the seed moves per process.

**Not every verb that works inline shows it**, which the sentence above invites. On the same shape,
`teleport actor 1`, `varcube 0 7` and `behaviour 2` each replay clean over 301 ticks with the command
a step out of position, and all three move fields the digest mixes: with command execution disabled
outright, `teleport` goes red at tick 61 and a quiet session stays clean, so the arm can see them.
A step of clock is invisible to a verb that does not spend one, and a modal spends it through the
presents of its inner loop. The table above is a table of clock-spending verbs rather than of modals.

So this is a command-scheduling defect in the recorder and not a clock defect, and the modals are
only how it becomes visible: they are the commands that do enough work inside one tick to move the
digest.

**Three readings that suggest themselves are all wrong**, and each is recorded as dead because
each is reachable by a plausible route. It is not the `ui` capture verbs, because `slide` is
not one and behaves identically. It is not the missing `SaveTimer` bracket, which is a real
asymmetry -- `MenuInventory` is entered from [PERSO.CPP:1142](../../SOURCES/PERSO.CPP#L1142) inside
one and from [CONSOLE_CMD.CPP:539](../../SOURCES/CONSOLE/CONSOLE_CMD.CPP#L539) without one -- but
`cmd_slide` brackets its modal ([CONSOLE_CMD.CPP:1027](../../SOURCES/CONSOLE/CONSOLE_CMD.CPP#L1027)
and [:1048](../../SOURCES/CONSOLE/CONSOLE_CMD.CPP#L1048)) and diverges just the same. And it is not
the two ends disagreeing about whether the tick's free present had been spent when the modal opened:
the trace carries `FixedDtSkipPresent` at every call and it reads 1 on the tick's own render and 0
on every extra present, identically on both sides.

Two limits on the claim. Every modal above is opened by a console verb, because a player-path modal
cannot be driven headlessly: the inventory opens on `I_INVENTORY`, which `input` can supply, and
closes on `MyKey == K_ESC`, which comes from `Key` and not from the `TabKeys` that `key` pokes
([CONTROL.CPP:1350](../../SOURCES/CONTROL.CPP#L1350)). A player-opened modal has no console command
behind it, and a hand-played session that opens the inventory, selects and uses an item does replay
correctly, so the divergence above is a claim about the console entry path and not about modals.
And the `slide` row exists only because that wait now terminates under a pinned clock; before, there
was nothing to replay.

### The position is now the recording's, and the table is clean

The readers stage a command and neither of them runs it; two drains run what they staged, each from
the point in the frame the recording ran it from -- `Record_ExecHook` from `Control_TickHook` beside
`--exec-at`, and the tail of the poll. No format change: the writer already puts a command where it
ran, so the stream position says which of the two a command is in. Re-measured on the same shape,
`--exec-at 60`, 300 pinned ticks, replayed with `--tick 400`:

| Session | Replay |
|---|---|
| quiet, no modal | 301 ticks checked, no mismatch, 0 ms drift |
| `cube 154` | 301 ticks checked, no mismatch, 0 ms drift |
| `ui inventory` | 301 ticks checked, no mismatch, 0 ms drift |
| `ui dialog 1` | 301 ticks checked, no mismatch, 0 ms drift |
| `ui menu-main` | 301 ticks checked, no mismatch, 0 ms drift |
| `ui found-object 0` | 301 ticks checked, no mismatch, 0 ms drift |
| `ui holomap` | 301 ticks checked, no mismatch, 0 ms drift |
| `slide activision` | 301 ticks checked, no mismatch, 0 ms drift |

The other half of the same defect runs in the opposite direction and is not visible from this
table at all, because every row here is a command written at a tick boundary. A command written
mid-frame -- which is where the console writes, since it runs from the input path -- replayed a
whole tick late, and that one any digest-moving verb can see: driven through `--listen`, which
issues from the present path and lands in the same on-disk layout, `varcube 0 7` recorded at tick
155 ran at 156 and the digest failed at 155. The same verb is clean in the layout above.

### A modal can stall a replay with no clock in it anywhere

Enumerating modal loops by their clock source misses the one a player is most likely to hit.
`InputPlayerName` ([GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP)) reads its characters from
`GetAscii`, which used to read `SDL_GetKeyboardState(NULL)` directly rather than the polled key
table the recorder samples. `replay_inject` ([RECORD.CPP](../../SOURCES/RECORD.CPP)) writes
`TabKeys`, `Key`, the pad state, mouse motion and `Click`; SDL's own keyboard array was not among
them. So the save-name screen's exit condition was a keystroke that only a physically pressed key
could produce, and a replay of it waited for one that never came.

That loop has a present, a clock read and a sane clock source, so it was clean on every axis this
survey measures and stalled anyway. The modal loops are not only a clock problem.

`GetAscii` now takes the poll instead, which closes it. What the case leaves behind is the reason
it was worth enumerating: **the loop reached neither of the recorder's two seams, in either
direction.** It never called `MyGetInput`, so no poll hook ran and a replay had nothing to inject
into it; and a modal blocks `MainLoop`, so no tick hook ran and a command staged for it had nowhere
to land. A wait loop that takes no input and advances no tick is invisible to a poll-counting
deadlock detector as well, which is why the stall was reported as a hang rather than named by the
engine. Measured on one recording that reaches the screen, replayed at `--fixed-dt 16`: before the
change the run was killed at the 45s timeout with no recorder complaint in the log; after it, 6s,
`201 ticks checked, first hash mismatch -1, longest stale run 0`.

## What is safe without step B

The collapse itself is step B, so only these carry:

- Eight minted steps: the seven waits above, plus `GameOver`'s zoom loop. Two take
  `Timer_FixedDtPump` and six the polled form, on the split described above. Off the pinned clock
  the polled form compiles to two branch tests, so a shipping run is unaffected; on it, a loop that
  could not terminate now does.

  The safety argument has one real limit. "No existing recording can contain one of these loops"
  holds only for the two `GereArdoise` waits: the other five poll, so they run to completion in a
  loose-clock recording and can be in files recorded before this. That is the case the polled form
  exists for, and it is measured above rather than argued.
- Five tests in [test_fixed_step.cpp](../../tests/timer/test_fixed_step.cpp): the fade's
  two-mints-per-iteration shape, so a collapse that fixes it fails loudly rather than silently;
  a clock-terminated modal loop whose only clock is its own present, which is the counterexample in
  miniature; a clock wait with no source, which is the class above; that only the unpolled pump
  reaches the wait hook, which is the split; and that arming the pinned clock clears a pending
  overlay claim, which it did not.
- `Timer_EnableFixedDt` now clears `FixedDtOverlayPresent` along with the other flags. An overlay
  claimed and never presented used to outlive a re-arm and swallow the step of the first present
  after it, which is a tick of simulation time that silently does not happen.
- A correction to step A in [ENGINE_RENDER_SPLIT_RESEARCH.md](ENGINE_RENDER_SPLIT_RESEARCH.md).

## Where this disagrees with the ladder doc

[ENGINE_RENDER_SPLIT_RESEARCH.md](ENGINE_RENDER_SPLIT_RESEARCH.md) prices step A as small,
behaviour-neutral, and as retiring three things. Three corrections, in descending order of how much
they matter:

1. **"Changes behaviour: no" is false.** Presents that never mint deadlock `OpenInventory` and the
   end credits, measured above. Step A as written is not a consolidation, it is a deadlock.
2. **The deletions belong to step B.** `Timer_FixedDtPresent` and `Timer_FixedDtOverlayPresent`
   cannot go until every clock-terminated loop has a call of its own, and that call is step B's.
   The ladder's ordering has the two the wrong way round.
3. **The recorder's frame-clock latch is not step A's to retire either, and for a different
   reason.** `Record_ClockHook` returns before reading anything while the pinned clock is armed
   ([RECORD.CPP:2658](../../SOURCES/RECORD.CPP#L2658)); it exists for the host-sampled clock, where
   there is no tick to hook either. An engine tick would give it a better place to latch than the
   input poll, but the latch itself is what makes a loose replay possible and it does not go away.
   "Moves" rather than "retires".

## Related

- [ENGINE_RENDER_SPLIT_RESEARCH.md](ENGINE_RENDER_SPLIT_RESEARCH.md) -- the ladder this is step A
  of, and the inventory of what still ties the renderer to the simulation.
- [TIMING.md](../TIMING.md) -- canonical for the two clocks, lock versus save, and the overlay
  present.
- [FIXED_DT_PLAN.md](FIXED_DT_PLAN.md) -- the pinned clock these policies act on.
- [RECORDING.md](../RECORDING.md) -- what a recording pays for the clock, and the console incident
  that produced `Timer_FixedDtOverlayPresent`.
- [MOVEMENT_FRAMERATE.md](../MOVEMENT_FRAMERATE.md) -- the #358 fixed-timestep loop, the half of
  the split that shipped.

## Reproduce

The static counts:

```sh
# present sites outside DIRTYBOX (142)
grep -rn "BoxUpdate()\|BoxBlit()\|BoxStaticFullflip()" --include=*.CPP --include=*.C \
  SOURCES LIB386 | grep -v "^LIB386/SVGA/DIRTYBOX.CPP" | wc -l

# every direct caller of the four policies
grep -rn "Timer_FixedDtAdvance\|Timer_FixedDtPresent\|Timer_FixedDtOverlayPresent\|Timer_FixedDtPump" \
  --include=*.CPP SOURCES LIB386

# the only present path: no caller outside DIRTYBOX
grep -rn "LockVideoSurface\|UnlockVideoSurface" --include=*.CPP SOURCES LIB386
```

The loop scans are two brace-matching walks over `SOURCES` and `LIB386`, both capped at 400 lines
so both results are floors. The first keeps loops containing a present and a read of
`TimerRefHR`/`TimerSystemHR` (29). The second keeps loops that call `ManageTime` and read the clock
with no present, pump or delay anywhere in the body: 14 on `51fe3890`, of which seven are
reachable under a pinned clock, five are boot-only and two are false positives from a helper that
presents. The pump calls below take the seven out, leaving 8.

The mint counts come from a temporary instrument, not from anything in the tree: a counter in
`FixedDtStep` incremented per policy, a per-tick histogram reset in `Timer_FixedDtAdvance`, and a
site table keyed on `__builtin_return_address(0)` captured in `BoxUpdate` and `BoxStaticFullflip`
and read in `BoxBlit` (so the outermost of the three names the game-side caller), dumped at
`atexit` and symbolised with `addr2line` against a `RelWithDebInfo` build run under `setarch -R`.
`addr2line` resolves a return address, so it names the statement after the call: every site in the
tables above was read back from the source and cited at the call itself, one line lower than the
tool printed.
The counterexample adds one branch in `Timer_FixedDtPresent` that skips `FixedDtStep`, gated on an
environment variable, and is driven as:

```sh
# control and arm, same binary
env -u LBA2_NO_PRESENT_TICK  lba2cc --headless --no-autosave --language English \
  --resolution 640x480 --fixed-dt 16 --load "$SAVE" --exec-at 60 "ui inventory /tmp/x.png" \
  --tick 300 --exit
LBA2_NO_PRESENT_TICK=1       lba2cc ...same...

# the wait with no clock source, driven
lba2cc --headless --no-autosave --load "$SAVE" --exec-at 60 "slide activision" --tick 300 --exit
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 16 --exec-at 60 "slide activision" \
  --tick 300 --exit
```

The paced wall-clock figures use a clean build with no instrument, `--record` to arm pacing, and
`date +%s%N` around the run.
