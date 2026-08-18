# Input: plan

An order for making input a subsystem: owned, tested, complete across its devices, and
expressible as data. Each increment ships on its own, and each one makes the next cheaper or
less risky.

Follows [INPUT_RESEARCH.md](INPUT_RESEARCH.md), which describes what is there now, with
[INPUT_DOOM3_RESEARCH.md](INPUT_DOOM3_RESEARCH.md) as the outside comparison and
[REFACTOR_ROADMAP.md](REFACTOR_ROADMAP.md) area 8 as the case for doing it at all. The recipe
each increment follows is [FEATURE_WORKFLOW.md](../FEATURE_WORKFLOW.md) Example 5.

## What this plan is not

**It does not deliver a new control scheme.** #372's omnidirectional proposal and #4's
Enhanced-Edition-style controls are out of scope, and the research is why rather than a
preference: both are blocked on things that are not input.

Hero locomotion is animation-driven. `Input & I_UP` calls
`InitAnim(GEN_ANIM_MARCHE, ANIM_REPEAT, numobj)` and the displacement comes from the animation's
keyframes, so there is no speed scalar for a stick to fill and no `forwardmove` field to put one
in. Camera-relative movement then needs the exterior camera, where `BetaCam` is variable and is
taken away by camera zones and cutscenes. Neither of those is fixed by anything below.

That is a scope line, not a refusal. Everything below makes the scheme question **cheaper to
experiment with**, because a binding set that can be swapped whole is how an alternative scheme
would ship anyway, and a tested binding layer is what makes trying one reversible.

## Where input already is

Better placed than area 8 implies, in one specific way. The prerequisite most refactor areas
share, de-aggregating a module header out of `DEFINES.H`, does not apply:
`//#include "INPUT.H"` is already commented out there, 18 files include
[SOURCES/INPUT.H](../../SOURCES/INPUT.H) explicitly, and none of its state
(`DefKeys`, `GamepadKeys`, `LastInput`, `NbInput`) is declared in
[C_EXTERN.H](../../SOURCES/C_EXTERN.H). Input has already had its ownership move.

So this plan is not about untangling a god header. What is actually missing is narrower and more
tractable: no test on the binding table, suppression that nobody owns, three devices on a path
that cannot be bound, and a localised key-name table shipping unread since 1997.

## The rule about parity

The keyboard layer has a parity obligation to retail, and increment 0 is where that stops being a
claim. The rule this plan follows:

**Freeze observable behaviour. List the exceptions. Every exception arrives with the test that
says why it changed.**

Frozen: `DefKeysDefault95`'s action-to-key mapping, and the bits `GetInput` produces for a given
key state including `NoRepeatInput`'s masking. Free: everything behind those, including how the
combined table is built and where the code lives.

Two exceptions are already known and expected to be taken, each in the increment that owns it:

| Exception | Where | Why |
|---|---|---|
| `K_NUMPAD_POINT` offered but unpressable (#508) | increment 3 | the alias names `SDL_SCANCODE_KP_DECIMAL`; hardware sends `KP_PERIOD` |
| A rebind drops the action's keypad twin (#507) | increment 4 | the twin is a layout fact sharing a slot with a preference |

## The refactor, named

[REFACTOR_ROADMAP.md](REFACTOR_ROADMAP.md) area 8 does not ask for correctness work. It asks for an
extraction, on the pattern the camera used: *"Which physical keys carry which intent, and which
action a profile puts where, are both pure lookups over a table, so they can come out as a header
with a host test in the way the camera's angle arithmetic did."* The increments below do that work
but never said what comes out, so this section says it.

**Step 1 of [FEATURE_WORKFLOW.md](../FEATURE_WORKFLOW.md) Example 5 is to count what CI can see of
the code about to move, before anything else.** Measured on
[SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP), 446 lines, of which none is under any test:

| Part | Lines | Reads engine globals or IO |
|---|---|---|
| `DefKeysDefault95`, the retail layout | 41 | no |
| `GamepadKeysDefault` and the live tables | 55 | no |
| `InitInput`, the fold of two tables into the combined one | 24 | no |
| `RestoreInput` | 9 | no |
| **pure total** | **150** | |
| `ReadInputConfig` / `WriteInputConfig` / the two gamepad halves | 179 | `DefFileBuffer` IO |
| `MyGetInput` | 75 | console, joystick, `TabKeys`, `Key` |
| `WaitNoInput` / `WaitInput` | 24 | polls |

**The testable line is therefore clean**, and the extraction is 150 lines: the tables and the fold
come out, the config IO and the polling stay. (Done in increment 0, and 132 lines once the blank
lines the estimate counted are taken out.) Proposed name `INPUT_BINDINGS.{H,CPP}`, on the naming
pattern of `AUDIO_BALANCE`, `SAVEGAME_WIRE` and `RES_DISCOVERY`. The layout half of increment 4
grows out of [MENU_KEYNAV.CPP](../../SOURCES/MENU_KEYNAV.CPP), which area 8 already calls the first
piece of it.

**The justification is better than the coverage arithmetic suggests.** 150 lines moves
`SOURCES/` from 6% covered to about 6.6%, which satisfies the roadmap's standing rule and is not
in itself interesting. What is interesting is *which* 150 lines. `DefKeysDefault95`, `InitInput`
and `RestoreInput` are all present in the 1997 import, and the roadmap's own headline finding is
that **not one line of 1997 game logic is currently reachable from a host test**. Testing the
retail key table and the fold that builds `Input` from it would be the first. That is the
different justification the roadmap asks for from any refactor that does not move the percentage
much, and it is a stronger claim than the percentage.

A detail worth keeping while the table is being moved: the 1997 file carried **two** default
layouts, `DefKeysDefault` and `DefKeysDefault95`, a DOS set and a Windows 95 set. This port kept
only the second. An extracted table is the natural place to record that, and a binding set
(increment 6) is the natural place for the first one to come back if anyone wants it. It is
recorded above `DefKeysDefault95` in the extracted file.

### Which increment does which step of the recipe

| Recipe step | Where it happens |
|---|---|
| 1. Count what CI can see before moving | the table above, and increment 0 |
| 2. CODESTYLE "where new code goes" | increment 0, when the new TU is created |
| 3. Cut along the testable line first | the 150 pure lines, leaving IO and polling behind |
| 4. Make the extracted part correct on its own terms | increment 0's round-trip and fold tests |
| 5. **Move before you change** | increment 0, second commit: same lines, new home, no behaviour change |
| 6. Then ownership | already true. `INPUT.H` is out of the `DEFINES.H` aggregation with 18 explicit includers |
| 7. Then surfaces, one entry point each | increment 3 (the key names the config screen shows), increment 6 (cfg reader and writer) |
| 8. Bugs found on the way get their own commit | the two parity exceptions above, each with its own test |
| 9. Write the rule down as you find it | this plan and [SPEEDRUN_MECHANICS.md](../SPEEDRUN_MECHANICS.md) |
| 10. Re-read every doc that describes what moved | [CONFIG.md](../CONFIG.md), [CONTROLLER.md](../CONTROLLER.md), [MENU.md](../MENU.md) |

Step 5 is the one nothing in this plan previously carried, and it is the one that makes the rest
safe: the commit that creates `INPUT_BINDINGS.{H,CPP}` must be a pure move with the tests from
increment 0 moving alongside it, so that increments 3 and 4 change a module that is already under
test in its own translation unit rather than editing 1997 code in place.

## Two decisions this plan defers

Both are real forks, and neither should be settled by preference when an increment can settle it
by measurement. They are listed here so that the increment which answers each says so.

**Do the two funnels converge?** 154 sites read the action bits, 93 compare raw scancodes, and
three devices depend on the raw path by design. Converging means routing the raw sites; not
converging means formalising the second path so a screen inherits the legend instead of restating
it. **Increment 4 answers it**, because once layout is a real layer the 93 sites can be counted by
which of them wanted a binding and which only wanted the legend.

**How much test machinery gets built?** Consumption is what host tests cannot reach, and Doom 3's
answer (record commands, replay, compare a per-tic state hash) is genuinely enabling and also the
most plausible candidate for overengineering here. **Increments 0 and 1 answer it**, by covering the pure layers and then measuring whether the
fixtures plus the flow counters catch what the later increments could break.

## Injection: the harness is not a device

Four things put input into this engine and only three of them are injection paths, which is worth
settling before increment 7 treats them alike.

| Path | Enters as | Where | Metered in |
|---|---|---|---|
| `ApplyVirtualKeys` (touch overlay) | scancodes in `TabKeys` | inside `UpdateKeyboardState`, before the scan | held while the finger is down |
| `ApplyHarnessKeys` (`key` verb) | scancodes in `TabKeys` | same hook site, immediately after | **input polls**, one per `ManageKeyboard` |
| `DbgInjectInput` (`input` verb) | action bits in `Input` | `MainLoop`, after `MyGetInput` | **sim ticks** |
| `--listen` socket | nothing | `Control_ServiceListen`, in front of the console bus | **presents** |

**The socket is a transport, not a fourth path.** It delivers the same `key` and `input` verbs a
`--exec` line would. What it adds is a third clock: a command *arrives* on a present, and then
meters itself in polls or ticks depending on which verb it was. So one driver on one socket runs on
two different time bases, and the present-versus-tick trap already documented in
[CONTROL.md](../CONTROL.md) is the first of three rather than a standalone caveat. The set should
be written down there.

**The rule that makes the rest coherent is the inverse of the device rule.**

A *device* should name an **action** and let the binding layer resolve it. The touch overlay naming
a scancode is exactly G1: rebind the keyboard and the on-screen D-pad stops working. The pad
synthesising `K_UP` for menus is G2.

The *harness* should name a **layer**, deliberately. `key` proves the binding layer itself works
(rebind an action, press the new key, watch the hero); `input` proves consumption works
independently of whatever is bound. Neither is redundant and neither is a workaround: they are
probes at two different boundaries, and a harness with only one of them could not test half of
this subsystem. That they inject at different levels is the feature.

So increment 7 moves the devices onto the binding layer and leaves the harness spanning it.

**Two things this leaves open.**

*No action-level injection reaches a modal.* `input` ORs into `Input` from `MainLoop`, which a
screen spinning its own `MyGetInput` loop never reaches;
[INPUT_SIM_PLAN.md](INPUT_SIM_PLAN.md) resolves that by reaching for `key` instead. That is fine
today and stops being fine at increment 4, because `key` deliberately bypasses the binding layer
and a menu is where you would most want to assert that a *binding* resolved. A third verb naming a
resolved action belongs with that increment, as test machinery rather than as a feature.

*The two hooks cannot see each other.* `ApplyVirtualKeys` runs first and `ApplyHarnessKeys`
second, and on expiry the harness lifts its key explicitly with `control_key_poke(sc, 0)` rather
than waiting for the next clear. If touch is holding the same scancode, that lift clears it. It
needs the harness and the overlay live together on the same key, so it is narrow, but the comment
at the hook site saying the two "do not have to know about each other" is precisely why it is
possible.

**A naming collision, on the pattern of "profile".** `ApplyVirtualKeys` is the touch overlay's
synthetic scancode state, an implementation detail of one device. The *virtual keyboard* a
non-keyboard player needs in order to type a save name (G4, the `TODO(input)` at
[GAMEMENU.CPP:1175](../../SOURCES/GAMEMENU.CPP)) does not exist and is unrelated. Two different
things under one word, and the second one has no owner in this plan.

## Three fixes that should not wait for any of this

Not increments. Independent bug fixes, each small, none blocked on the plan:

- **#509**, the disc prompt cannot be answered with a gamepad. `ConfirmMenu` reads only `MyKey`
  and matches no pad scancode, so a pad-only player sits in `while (flag == -1)` with no way out
  and `CDROM` is unconditional in the default build. This is a lock, and it should go first of
  everything on this page.
- **#508**, the Key2 column is unreachable from the keyboard on the gamepad screen, because the
  screen's key filter swallows Left and Right before the line that would toggle the column. A
  player is on that screen precisely when the pad is not working.
- **#514**, orbit follow-through applies to the mouse, which does not spring back to centre.

## The increments

### 0. Put the binding table under test

**What.** [SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP) into a host test, which nothing links
today. Three assertions: `DefKeysDefault95` as data; `InitInput`'s folding of two tables into the
combined 128-entry one, including that slots 32 to 35 do not reach `Input`; and
`ReadInputConfig`/`WriteInputConfig` as a round trip, including the all-zero-bindings guard and
the first-exit case the code carries comments about.

**What it de-risks.** Everything after it. The binding table is the one layer with no test sitting
between two that have one (`tests/input_device` below it, `tests/menu_keynav` above it), and every
increment here edits it. It is also where the parity rule becomes an artifact instead of a
sentence.

**How it is proven.** It is the proof. No playtest needed.

**What it costs.** Small, and no behaviour change. The module header is already owned, so there is
no de-aggregation to do first.

**Two commits, in this order.** First the tests against `SOURCES/INPUT.CPP` where it stands. Then
the pure move of the 150 lines into `INPUT_BINDINGS.{H,CPP}`, same lines and same behaviour, tests
travelling with them. That second commit is step 5 of the recipe and it is what lets every
increment after this one edit a tested module rather than 1997 code in place.

**What it decides.** The deferred test-investment question. If covering the pure layers leaves the
fixtures as the obvious bottleneck for the increments below, replay earns a second look; if not,
it does not.

**Landed.** [tests/input_bindings/](../../tests/input_bindings/) holds the two, and
[SOURCES/INPUT_BINDINGS.{H,CPP}](../../SOURCES/INPUT_BINDINGS.CPP) holds what came out. Three
things came out of doing it.

*The move was 132 lines, not the 150 estimated above.* The difference is the blank lines and the
banner comment the estimate counted; the code is the same code, and a diff of the moved text
against its old home is empty. `INPUT.CPP` is 446 lines to 314.

*The layout differs from the 1997 import in exactly one cell.* `I_CAMERA`'s second binding was
`K_CARRE`, the backtick, and the console toggle took that key. Every other cell of
`DefKeysDefault95` is the 1997 table unchanged, which is a stronger parity claim than this doc
made and is now the test's to keep.

*The fold is provable through `GetInput` alone.* Holding one scancode and reading `Input` covers
the whole combined table without inspecting it, which means increments 3 and 4 can change how the
table is built and keep the same assertions. That is also how the 32-slot ceiling finally got
stated: rebinding the four spell slots to unused scancodes shows the ceiling follows the slot, not
the key.

*On the test-investment question it decides:* the fixtures were not the bottleneck here, because
these layers need none. What the layers below the fold need is a different thing, and increment 1
is what measures it.

### 1. Watch the signal flow, and record the combos before anything moves

**What.** Two halves, both read-only, both before any behaviour changes.

*A per-boundary flow counter.* Input crosses three boundaries and each can lose a signal for a
different reason. Count transitions on both sides of each and report the mismatch:

| Boundary | Loses a signal when | Counted against |
|---|---|---|
| SDL event to polled state | a press and release both land between two polls, so the level sample never saw it | `SDL_EVENT_KEY_DOWN`/`UP` in `HandleEventsKeyboard` against rising edges in `TabKeys` |
| polled state to action bits | a key is held that no binding names, including the spell slots that never reach `Input` | `TabKeys` rising edges against `Input` rising bits, with `NoRepeatInput`'s deliberate masking excluded |
| action bits to the sim | the throttle skips the frame the edge fell on | `Input ^ LastInput` per rendered frame against what a stepped frame consumed |

The first of those is nearly free: `HandleEventsKeyboard`
([KEYBOARD.CPP:120](../../LIB386/SYSTEM/KEYBOARD.CPP)) already receives every key event and, by an
explicit comment, keeps none of them. It is the only measurement that can answer whether level
sampling loses anything in practice, which the research left open.

`input trace` is not this. It logs the *injected* mask and hero state per sim tick, which serves
the simulator; this counts the *real* signal at each layer and reports disagreement.

*Combo fixtures, recorded as a baseline.* The `fseq` verb from
[INPUT_SIM_PLAN.md](INPUT_SIM_PLAN.md) Phase 1 already places an edge on a chosen frame, which is
what these need. The speedrunning community supplied the first group; the rest came from reading
every branch in `MOVE_MANUAL` that tests an input combination or tests input against animation
state, and three of them the community does not appear to have named.

Why each one works, with the original comments, is
[SPEEDRUN_MECHANICS.md](../SPEEDRUN_MECHANICS.md). That doc is written for the runners rather
than for this plan, and it is the reason these are a specification and not a wish list: people
depend on every row below.

**Behaviour-coupled.** The family speedrunners actually use.

| Combo | Why it probes input | Where |
|---|---|---|
| Running jump | Sporty plus `I_ACTION_M` on `GEN_ANIM_MARCHE` at `LastFrame == 0` picks `DO_LEFT_JUMP`, `DO_RIGHT_JUMP` or `DO_NORMAL_JUMP` by which foot is down, so the outcome is frame-exact | OBJECT.CPP `C_SPORTIF` |
| Behaviour press cancels a landing | the community's core trick. `SetComportement` sets `GenAnim = NO_ANIM` and `FlagAnim = 0`, so a press on the landing frame clears the recovery | OBJECT.CPP:889-899 |
| Sporty and Aggressive alternated between hits | keeps Twinsen close as enemies recoil; a late edge changes the exchange visibly | same |
| Two behaviour keys in one frame | `I_NORMAL`, `I_SPORTIF`, `I_AGRESSIF`, `I_DISCRET` are an if/else-if chain, so the *first listed* wins rather than the one pressed. Nobody has written that down | PERSO.CPP:1441-1447 |

**Hold against re-press.** The richest group, and the one most exposed to a lost edge, because
these paths read input *history* rather than input state.

| Combo | Why it probes input | Where |
|---|---|---|
| Up or Down held across a jump | if the bit was already in `LastMyJoy` the frame is skipped and the jump survives; release and re-press and it is not, so the jump prep cancels. The original comment says exactly this: *"si le joueur n'a pas relacher Up et Down avant de reappuyer dessus"* | OBJECT.CPP:4392-4413 |
| Aggressive attack held against re-pressed | held gives the same blow every time, because `(LastInput & I_ACTION_M) AND GenAnim != GEN_ANIM_RIEN` skips the `MyRnd(3)` draw. The 1997 comment calls it a bug: *"Twinsen combat toujours avec le meme coup"* | OBJECT.CPP:4113 |
| Direction changed while climbing | a change against `LastMyJoy` resets the animation on a ladder | OBJECT.CPP:4424 |
| Any movement or fire change after an action | `((Input & I_JOY) != LastMyJoy) OR ((Input & I_FIRE) != LastMyFire)` resets to idle, and already increments `DbgHeroActionResets` | OBJECT.CPP:4415 |

**Simultaneous opposites.** Undocumented anywhere, and the group a stick quantiser or a binding
change could flip without anyone noticing.

| Combo | Resolution today | Where |
|---|---|---|
| Up and Down together | Up wins: `if (Input & I_UP) ... else test_down = TRUE` | OBJECT.CPP:4440,4464 |
| Left and Right together | Left wins, in both the turn block and `ManualRealAngle` | OBJECT.CPP:3864, 4481 |

**Modifier and cross-layer.**

| Combo | Why it probes input | Where |
|---|---|---|
| Direction plus `I_ESQUIVE` | selects the four `GEN_ANIM_ESQUIVE_*` variants, with a separate branch for sidestep plus a turn | OBJECT.CPP:4383 |
| Direction held plus a spell | the spell keys bypass `Input` entirely through `SpellKeyDown`, so this probes the hole in the binding table | PERSO.CPP:1361-1407 |
| Walk and turn together | the arc: turning is velocity times dt and frame-rate independent, walking is animation-driven and is not, so the radius may change with frame rate | INPUT_RESEARCH open question 6 |
| Any of the above in the buggy | `BUGGY.CPP:277` carries its own copy of the re-press test, so vehicle input is a second path with the same shape | BUGGY.CPP:277 |

**Two constraints these carry.**

The Aggressive-attack case is an **original bug**, and its comment says so in 1997 French. A
fixture pins the buggy behaviour; it does not fix it. [BIT_EXACTNESS.md](../BIT_EXACTNESS.md)
decides what may change here and the answer is nothing.

Anything pressing attack draws from `MyRnd(3)`, and the RNG is one shared stream, so an attack
fixture perturbs it for everything downstream. Those fixtures need a fixed seed and should assert
on the reset counters rather than on which of the three animations was drawn.

**The counter precedent already exists.** `DbgHeroActionResets` counts one of these paths and is
already reported by `--dump-state` as `action_resets`, added for #456. The flow counters above are
the same idea applied at the layer boundaries rather than at one consumer.

**What it de-risks.** Everything after it, in the way the plan cares about: it is the baseline that
says what the engine did *before* increments 2 to 7 touched it, at the exact places where a
regression would otherwise be invisible. Behaviour cancels and foot-dependent jumps are precisely
what an input change breaks without anyone noticing until a player complains.

**How it is proven.** It is instrumentation, so the proof is that it agrees with itself: the
counters must show zero mismatch on the paths the existing throttle tests already cover, and the
combo fixtures must pass on today's engine before they are worth anything.

**What it costs.** Small to medium, and none of it changes engine behaviour. The counters are three
pairs of integers and a report; the fixtures are `fseq` scripts plus the traces the harness
already has.

**What it decides.** More than its size suggests. It measures the second boundary, which is the
two-funnel question stated as a number rather than a count of grep hits. It measures the third,
which tells increment 5 whether a static guard is still needed or whether the counter is the
guard. And it is the concrete form of the deferred test-investment question: if the fixtures plus
these counters catch what the increments below could break, replay is not needed.

### 2. Give suppression an owner

**What.** Replace the single file-static `NoRepeatInput` with a per-subsystem mask on Doom 3's
`InhibitUsercmd( subsystem, bool )` shape: each of console, menu, dialogue, holomap and cutscene
owns one bit and can only set or clear its own. The old latch and the new mask coexist, so call
sites convert a few at a time and the increment can stop anywhere.

Take the discard rule with it: while inhibited, accumulated input is dropped rather than banked,
so closing a menu cannot deliver a backlog.

**What it de-risks.** The class where one screen's suppression is cleared by another's exit, over
109 macro call sites that nobody owns. It also gives the console's hand-written suppression inside
`MyGetInput` (a `memset`, an `Input = 0`, a one-frame flag) a general mechanism to be the first
user of rather than a special case to be worked around by every later screen.

**How it is proven.** Extends `tests/input_funnel`, which already builds a synthetic binding table
and pins the mid-frame-rebuild regression. Playtest: menu and dialogue entry and exit, on keyboard
and pad.

**What it costs.** Small for the mechanism, incremental for the conversion.

### 3. Name the keys

**What.** Wire the localised key-name table the game already ships. Ids 200 to 268 in the `sys`
bank are 69 key names in all six languages, and the engine reads four of them, for the legacy
joystick axis path. This increment adds the scancode-to-text-id map and uses it for keyboard
bindings, falling back to `GetKeyScancodeName` for what the table does not cover (letters and
Escape).

**What it de-risks.** It is the first half of #61 at a fraction of its estimate, because the
strings exist and are already translated. It also puts a name on a binding, which is what
increments 4 and 6 need in order to express one as data rather than as a slot index.

**How it is proven.** The map is a pure bidirectional lookup, which is the shape
`tests/menu_keynav` already demonstrates: 40 host-test cases over 33 lines of source. Playtest: the
key config screen in more than one language.

**What it costs.** Small. A 69-entry table and a fallback.

**Exception taken here.** #508's keypad-period alias, which is a key identity question rather than
a screen question.

**Not included.** #61's other half, names in the cfg file. These ids are LBA's own, not a text form
of a scancode, so a written config wants its own vocabulary. That is a later step and it belongs
with increment 6.

### 4. Split layout from preference

**What.** #507. The `{Key1, Key2}` pair does two unrelated jobs: `{K_GRAY_UP, K_NUMPAD_8}` is a
layout fact and `{K_W, K_GRAY_END}` is a preference. Separate them, so a rebind cannot drop a
layout twin, and grow [MENU_KEYNAV.CPP](../../SOURCES/MENU_KEYNAV.CPP) from the keypad-only helper
it is into the layout layer that holds the rest (modifier pairs, and the keypad legend it already
covers).

**What it de-risks.** The defect class behind #497, #506 and #508, where a screen restates part of
the legend and silently loses what it left out. And it is the prerequisite for increment 6: a
binding set that could not hold layout facts separately would either duplicate them into every set
or lose them.

**How it is proven.** The layout layer is a pure lookup and joins `tests/menu_keynav`. The split
itself is proven by increment 0's round-trip test plus a new case per exception. Playtest: rebinding
with a keypad, both config screens.

**What it costs.** Medium, and it is the first increment where the cfg format changes, so it needs
a migration that reads the old shape.

**What it decides.** The deferred funnel question. With a layout layer in place, each of the 93
raw-scancode sites can be sorted into wanted-a-binding, wanted-only-the-legend, or genuinely
physical (a console toggle, a debug key). That count is the answer, and it is not knowable before
this lands.

### 5. Guard the throttle whitelist

**What.** A baseline of the post-gate `Input` edge-read sites, checked in CI, on the pattern
`scripts/ci/warnings-baseline.txt` already established. A new post-gate edge consumer fails the
check until its bits are in `I_ACTION_EDGE` or the baseline is updated deliberately.

**Possibly already done by increment 1.** That increment's third boundary counts the same loss at
runtime, and a runtime count is the more truthful of the two: a grep baseline can only see sites
it recognises. The static check earns its place only if it catches something the counter cannot,
which is a consumer no fixture exercises. Decide once the counters have run, and skip this
increment if they cover it.

**Why only a guard.** [INPUT_SIM_PLAN.md](INPUT_SIM_PLAN.md) Phase 2 measured this and chose not
to build the latch: the whitelist is complete, so the latch would be a pure refactor carrying
byte-exact risk for zero live bugs, and `test_melee_throttle.sh`,
`test_blowgun_release_throttle.sh` and `test_item_use_throttle.sh` were built instead. That
decision stands. Its stated trigger was "the day a future post-gate edge consumer lands outside the
whitelist", and nothing detects that day arriving. This increment is that detector and nothing
more.

**It also closes an open question in that plan.** The spell edges (`I_PINGOUIN`, `I_JETPACK`,
`I_PROTECTION`, `I_FOUDRE`) were listed as outside both the whitelist and the latch, with their
scope unconfirmed. They are read through `SpellKeyDown` at
[PERSO.CPP:1361-1407](../../SOURCES/PERSO.CPP#L1361), before the throttle gate at line 1618, so
they run every rendered frame and cannot be dropped. Genuinely out of scope, and the record should
say so.

**What it costs.** Small. No engine change.

### 6. Binding sets

**What.** A named set of bindings covering keyboard and pad together, swappable whole, with the
layout facts from increment 4 held outside it so no set has to carry them. The existing cfg keys
become the default set.

**Naming, to settle when this starts.** "Profile" is taken: `--profile <name>` already means a
user-directory profile, and `ChoosePlayerName` calls a save a profile too. A third meaning would be
one too many.

**What it de-risks.** It is the shape #4 and #372 would ship into, whenever their direction call is
made, which is what keeps the scope line above from being a dead end. It is also where #61's cfg
half belongs, because a set written to disk wants readable names by then.

**How it is proven.** Set selection and serialisation are pure and host-testable. Playtest: real,
and the first increment where a community tester adds something a host test cannot.

**What it costs.** Medium to large: cfg format, a UI, and defaults.

### 7. Sources onto the binding layer

**What.** The two *devices* still on hardcoded scancodes (the harness stays spanning the layer, per the section above). The touch overlay presses default
scancodes from a static table, so a keyboard rebind silently stops the on-screen D-pad working
(G1). The pad synthesises `K_UP` and `K_ENTER` for menu navigation, which is a second, hardcoded
binding table that no config screen mentions (G2). Both should read the layer that increments 3
and 4 build. #359's 8-way virtual stick lands here too, and is cheap: the pad's own quantiser
already produces exactly the eight directions the hero can hear.

**Why last.** Both need somewhere to read a binding *from*, which is increment 4.

**How it is proven, and this constrains the increment.** There is no Android device on the
maintainer's desk, so the touch half has to be self-provable: the mapping becomes a pure lookup
with a host test, and the device pass is confirmation rather than the proof. #359 was filed from a
CMF Phone 1 on Android 15, so its reporter is the route for that confirmation. Until someone runs
it,
**G1 stays a reasoned finding rather than a confirmed bug**, and the plan should not claim
otherwise.

**What it costs.** Medium, and the touch half is gated on a tester rather than on effort.

## Play-testing

What each increment needs, given the answer that a pad on the desktop and community testers are
available and an Android device is not:

| Increment | Needs |
|---|---|
| 0, 1, 5 | nothing, they are checks and instruments |
| 2 | pad and keyboard, menu and dialogue transitions |
| 3 | a language other than English on the key config screen |
| 4 | a keyboard with a keypad, both config screens |
| 6 | real play, and the first place a community tester sees something a test cannot |
| 7 | an Android device, so a community tester, for the touch half only |

The three independent fixes each need a pad, and #509 needs a build with no keyboard reachable to
confirm the lock is gone.

## What this plan does not buy

**It does not make the game feel different.** Nothing above changes how the hero moves. The one
user-visible change in the whole plan is #509 unblocking a player who cannot currently proceed,
and that is listed as an independent fix rather than an increment.

**It does not settle the scheme question**, and increment 6 is the most it does toward it: a place
for an answer to live once the direction call is made.

**It does not raise the coverage percentage much.** The extraction is 150 lines against
`SOURCES/`'s 73,664, so roughly 6% becomes 6.6%. The roadmap's rule is satisfied on the letter and
the real argument is the one above: those 150 lines would be the first 1997 game logic any host
test has reached.

## How this doc changes

Edited as increments land, not rewritten at the end. The two deferred decisions above are the
places where it is expected to change most: increments 0, 1 and 4 each exist partly to
produce an answer, and if either answer contradicts the ordering here then the ordering is what
gives way.
