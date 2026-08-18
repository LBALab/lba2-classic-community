# Refactor roadmap

Which parts of the tree are worth restructuring, what each one actually buys, and the order
that follows from it. A companion to [CODESTYLE.md](../../CODESTYLE.md), which says what the
result should look like, and [FEATURE_WORKFLOW.md](../FEATURE_WORKFLOW.md) Example 5, which
says what order to do one in.

Measured against `main` on 2026-08-18. Numbers age; re-run the commands in each section rather
than trusting them.

## The rule that decides the order

**Test coverage gates everything.** A refactor is a promise that behaviour did not change, and
that promise is worth exactly what the tests behind it are worth. So an area's position here is
set by what covers it today, not by how untidy it is.

Where an area has no cover, that does not disqualify it. It means the first commit builds the
cover, and the area costs more than its line count suggests.

## What CI can see today

The ASM equivalence suite runs from a workflow of its own,
[.github/workflows/test.yml](../../.github/workflows/test.yml), which builds the cached image and
runs `run_tests_docker.sh` on every push and pull request, about 80 seconds on a cache hit. Worth
knowing before measuring what CI covers: grepping the three platform workflows for
`LBA2_BUILD_ASM_EQUIV_TESTS` finds it `OFF` in all of them and answers a different question, since
that flag is set inside the container rather than in their YAML.

| Scope | Lines | Linked into a test | Runs in CI |
|---|---|---|---|
| `SOURCES/` | 73,664 | 4,716 (**6%**), of 90 files | yes, host tests on 3 platforms |
| `LIB386/` | 43,230 | 7,290 (**16%**) | yes, host tests plus ASM equivalence |
| ASM equivalence (153 tests) | n/a | n/a | **yes**, `test.yml`, Docker, every push and PR |
| `tests/automation` (60 fixtures) | n/a | n/a | **no workflow references it** |

Two things follow, and they shape every section below.

**Every host-tested file in `SOURCES/` is a community TU.** Not one line of 1997 game logic is
reachable from a host test. The 6% is `CLI_ARGS`, `RES_*`, `SAVEGAME_WIRE`, `AUDIO_BALANCE` and
their neighbours, all written in this fork. What does cover 1997 code is the ASM equivalence suite,
and it covers `LIB386/` rather than the game logic in `SOURCES/`.

**One real gap is left, and it is the fixtures.** The 60 in `tests/automation` drive a real engine
and catch what nothing else can, but they need retail data and a display, so they run when someone
remembers. That is the coverage an area can look well served by on paper while being unguarded in
practice.

Reproduce:

```bash
grep -rhoE "(SOURCES|LIB386)/[A-Za-z_0-9/]+\.CPP" tests/*/CMakeLists.txt | sort -u  # what a test links
grep -rln "run_tests_docker.sh\|ctest" .github/workflows/                           # what CI runs
grep -rln "tests/automation" .github/workflows/                                      # nothing
```

## Growth since the 1997 import

The import is `333929ab`. How far each original file has drifted says where new work has been
landing, which is where the dialect rule stops being decidable:

| File | 1997 | now | delta |
|---|---|---|---|
| [SOURCES/GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP) | 3,521 | 5,292 | **+1,771 (+50%)** |
| [SOURCES/PERSO.CPP](../../SOURCES/PERSO.CPP) | 2,562 | 3,205 | +643 (+25%) |
| [SOURCES/SAVEGAME.CPP](../../SOURCES/SAVEGAME.CPP) | 1,544 | 2,015 | +471 (+30%) |
| [SOURCES/GRILLE.CPP](../../SOURCES/GRILLE.CPP) | 1,155 | 1,498 | +343 (+29%) |
| [SOURCES/CONFIG.CPP](../../SOURCES/CONFIG.CPP) | 1,273 | 1,455 | +182 (+14%) |
| [SOURCES/OBJECT.CPP](../../SOURCES/OBJECT.CPP) | 7,393 | 6,787 | -606 (-8%) |

CODESTYLE names PERSO.CPP as the cautionary case because it holds `main()`. By volume the worse
case is the game menu, which has taken nearly three times as many new lines.

The other standing figure: [SOURCES/C_EXTERN.H](../../SOURCES/C_EXTERN.H) declares 266 externs
and is included by 58 files, so most of these globals are reachable from most of the tree
without any file saying it uses them.

## The prerequisite most areas share

Ownership moves have two halves. The mechanical one is that a module's header
declares what its `.CPP` defines. The one worth having is that a caller reaching for
the module's state must include that header, so the dependency shows up in its
include list and everyone else loses the ability to touch it by accident.

The second half is currently unavailable to almost every module here.
[SOURCES/DEFINES.H](../../SOURCES/DEFINES.H) aggregates 46 local headers, 42 of them
modules with a matching `.CPP`; [SOURCES/C_EXTERN.H](../../SOURCES/C_EXTERN.H)
includes DEFINES.H; 58 files include that. Every module header is therefore already
in front of every translation unit, so moving declarations into one changes where
the text sits and nothing else.

The Auto camera did get the benefit, which is why this was not obvious: FOLLOWCAM.H
was a new file that DEFINES.H had never heard of. Every module that predates the
aggregation needs it removed first.

Measured once, on audio: about a dozen explicit includes, plus making the module
header self-contained, because an aggregated header can name types it never
included. AMBIANCE.H went from 3 explicit includers to 25. Budget the same for any
other module, and read it as roughly a day's work rather than a campaign.

This changes what areas 1, 3, 6 and 8 cost, since each of them is an ownership move
against a pre-existing module. It does not change their order.

Progress on it is measured rather than asserted. `check-arch.py` ratchets the *text*
of the two headers, which both the Auto camera and the cfg reader satisfied while
still opening with `C_EXTERN.H` and pulling ~190 headers apiece. The number that
moves when the aggregation actually recedes is how many translation units end up
with `DEFINES.H` in front of them, and it needs a compiled tree to see:

```bash
scripts/ci/check-build-graph.py --report   # 57 TUs today; median TU pulls 5 headers
```

`linux.yml`'s build job runs it on every push, so the figure cannot quietly rot.

---

## The areas

### 1. Audio ownership

**What.** Move the audio globals out of the god header into the module headers that already
exist: `SampleVolume`, `VoiceVolume`, `MasterVolume`, `SamplesEnable`, `ReverseStereo`,
`RestartMusic`, the three `ParmSample*`, `SampleAmbiance[]`, `CubeJingle`.

**Coverage today.** Good, and unusually so: `tests/ambiance_balance` already links
[SOURCES/AUDIO_BALANCE.CPP](../../SOURCES/AUDIO_BALANCE.CPP) straight into a host test, plus
`tests/music`, `focus_audio`, `cdda`, `cdtracks`, `voc_header`.

**What it buys.** The first domain out of the god header whole, and the cheapest proof that the
camera pattern generalises to something that was not designed for it. Most of the recipe is
already satisfied by accident: the module TUs exist ([SOURCES/AMBIANCE.CPP](../../SOURCES/AMBIANCE.CPP),
[SOURCES/MUSIC.CPP](../../SOURCES/MUSIC.CPP)), the pure part is already under CI, and the cfg
surface reaches these through the settings table rather than by hand. What is left is step 6,
ownership, in isolation.

**What it costs.** Small. Expect to land maybe 7 of the 11 and leave the rest: `SampleAmbiance[]`
and `CubeJingle` are written per cube by gameplay, and CODESTYLE's "state that is genuinely
shared stays shared" rule applies to them until an owner exists.

**Verdict: done, in PR #563.** It cost more than this section estimated, in two ways worth
carrying forward. The coverage audit that step 1 of the recipe now demands turned up a real
defect before any code moved: a negative `MusicVolume` in the cfg read back as 127, the loudest
setting, because `JingleVolume` is `U32` where its three siblings and the cfg reader are `S32`.
And the ownership move needed the de-aggregation above first, or it would have been bookkeeping.
Fourteen globals moved, five stayed on the shared bus with the header saying why, and the god
header went from 266 externs to 252.

Finished in #568 and #570, which did the step this section forgot to list: surfaces. The console
verbs moved to the module (#568), and the volumes became a declarative table with cvars (#570),
so audio now meets the cfg reader, the cfg writer and the console the way the camera does. Both
turned up further defects the same way the first did, by writing the coverage first: the console's
reverse-stereo verb applied without recording, and the CD volume carried the same wrap as the music
volume two lines below it.

The lesson for the sections below is that an area is not done at ownership. Every one of them
should read "and then its surfaces".

### 2. Boot and fatal-error plumbing

**What.** Lift the boot diagnostics out of PERSO.CPP into their own TU: `ErrorHQRGet`,
`TheEndCheckFile`, `InitTheEnd`, `TheEnd`, `DebugHQR`, `ShowFatalErrorDialog`, `BootFatal`,
`TheEndInfo`, both `Message` overloads. Roughly 275 contiguous lines.

**Coverage today.** Indirect only: `tests/cli_args`, `version`, `distrib`, `discovery`, `picker`
cover boot inputs, and the `cli_*` fixtures cover boot behaviour. The failure paths themselves
are untested, because reaching them means failing a boot.

**What it buys.** A boot failure path that can be asserted on. This is not hypothetical: bad or
partial installs are what users actually hit, and the history is there (cfg corruption dropping
resolution, incomplete VOX, missing HQRs, disc-image sources). Today the code that decides what
the player is told when that happens cannot be exercised without breaking an install by hand.
Also the item CODESTYLE explicitly still names as owed, now that config file IO has moved.

**What it costs.** Small, and low risk: it is a surface, so nothing depends on its state and the
dependency direction is trivial.

**What it does not include, and why.** The obvious next thought is that boot should own
`main()` and call into the game, rather than boot work accreting inside a 1997 file. That is the
right long-term shape, and this is deliberately not a step toward it.

Three things make the scope decidable. `main()` was in PERSO.CPP in 1997 as well, already 410
lines of boot work against today's 867, so its placement is original rather than accretion.
[ENGINE_GAME_SEAM.md](../ENGINE_GAME_SEAM.md) classifies `PERSO` as **engine**, not game: it is
the main loop and entity framework, and LBA1 has one too. So the layering is boot, then engine,
then a game selected by id; boot would host the engine, and PERSO's main loop is the part that
does not vary between titles.

Against that, today's `main()` is three separate things:

| | Examples | Belongs to |
|---|---|---|
| Platform bring-up | `Mem_ConfigureScreenBuffers`, `Res_LoadBootDimensions`, `SetEventFilter`, `Perftrace_Init` | boot, and game-neutral today |
| Asset acquisition | fourteen `GetResPath` calls, `Load_HQR`, `InitDial`, `LoadListAnim3DS` | the game-id dispatch point |
| Handoff | `MainGameMenu()` | one line |

The first and third can move without anyone deciding who owns the entry point. The second must
not: [LBA1_PORT_PLAN.md](LBA1_PORT_PLAN.md) section 5 calls `GameId`/`GameProfile` the keystone
and the one dispatch point that selects per-game asset loading, and says it does not exist yet. A
boot module hardcoding LBA2's asset list would be building the wrong side of that seam.

Inverting control needs an interface that is not there either: the only "run the game" entry
today is `MainGameMenu()`, and defining what boot would call instead *is* the GameProfile. The
hosting question and the game-id question are one question, so neither is answered here.

**Verdict: do this second**, or first if a smaller blast radius matters more than the domain win.
Take the fatal-error and platform-bring-up halves; leave the asset half and the entry point for
whenever a game-id exists for a real reason. The cheapest route to that is the LBA1 feasibility
spikes, which touch no LBA2 code.

### 3. The game menu

**What.** [SOURCES/GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP), 5,292 lines and +50% since import.
The new material is resolution switching, mouse support, save-list handling, language selection
and the display sub-pages, sitting in and around 1997 menu code.

**Coverage today.** The best in the tree for a file this size: 14 `ui_*` fixtures, plus
`tests/menu_keynav`, `menu_labels`, `plasma_steps`, `ui_layout` as host tests. The tiered
comparison now runs to 1920x1080.

**What it buys.** This is the finding worth acting on. The largest accumulation of new code in an
original file is also the best covered surface, so it is far safer to restructure than its size
suggests, and the coverage was built for other reasons and is sitting unused for this. Splitting
the community additions into their own TUs would take the biggest single bite out of the
original-versus-new ambiguity anywhere in the tree.

**What it costs.** Medium to large, and it wants slicing: the sub-pages first, then mouse, then
the save list. The fixtures are local-only, so whoever does it has to run them deliberately.

**Verdict: the biggest prize, and only worth starting when someone has a run of time.** Not a
first move, but the one that pays most.

### 4. Remaining UI canvas surfaces

**What.** The roughly 26 sites in CONFIG, MESSAGE, HOLOGLOB, PERSO, INVENT and COMPORTE that
still spell the horizontal anchor inline, after the menu was converted.

**Coverage today.** Good for the menu and inventory, thin for the config and dialogue surfaces.

**What it buys.** Consistency, and little else. The rule already has one home and a host test;
these sites are correct, just verbose. FEATURE_WORKFLOW's own guidance applies: an unconverted
site is a known cost, not a regression.

**Verdict: opportunistic.** Convert a surface when already editing it. Not worth a dedicated PR.

### 5. Save

**What.** [SOURCES/SAVEGAME.CPP](../../SOURCES/SAVEGAME.CPP), +30% since import.

**Coverage today.** The strongest in the tree: `tests/save_wire` (including a fuzz test),
`tests/savegame`, `tests/save_thumbnail`, all host tests, plus the wire format pinned
bit-exactly.

**What it buys.** Very little right now. The growth is real but the risky part, the on-disk
format, was already extracted and pinned. This is what an area looks like after its refactor.

**Verdict: leave it.** It is here because the +30% invites a second look, and the answer is no.

### 6. World and cube transform

**What.** [SOURCES/GRILLE.CPP](../../SOURCES/GRILLE.CPP), +29%, plus the cube and block index
arithmetic around it.

**Coverage today.** `tests/zone` as a host test, some cube fixtures. Thin for the index maths.

**What it buys.** The open cube-change crash lives here, and so does the brick renumbering table
that #555 is currently fixing. A pure index seam with a host test, on the model of the camera and
UI ones, would make that class of bug expressible as a test instead of a repro script.

**What it costs.** Medium. Some of it is ASM-adjacent, so the bit-exactness rule applies and the
equivalence tests are the oracle, which means Docker.

**Verdict: after #555 lands**, and only with the crash still open as the motivation.

### 7. Rendering

**What.** Two layers with two different situations, which the first pass of this entry ran
together. The *primitives* are the `LIB386/` rendering libraries: the rasteriser, the 3D maths,
the blits. The *composition* is [SOURCES/OBJECT.CPP](../../SOURCES/OBJECT.CPP)'s `AffScene` and
`AffOneObject`, which decide what to draw.

OBJECT.CPP is not a rendering file. Of its 6,788 lines, the `Aff*` render path is 1,434; the
rest is the per-frame actor update (`Do*`, 1,440), collision and zones (`Check*`, 980), world
transitions (378) and 2,241 of everything else. It is the per-object frame loop, and rendering
is a fifth of it.

**Coverage today.** Excellent for the primitives, thinner than it looks for the composition. The
ASM equivalence suites run in CI on every push and pull request, but they are `LIB386` unit
equivalence: they assemble the original ASM and compare it against its C++ port, routine by
routine. **No ASM-equivalence test links `SOURCES/OBJECT.CPP`.**

What covers the composition is polyrec, which records the exact draw-call stream of a frame.
It has been used to compare the ASM and C++ fillers, but the stream it records is equally a
before-and-after oracle: a refactor that preserves behaviour produces an identical stream, byte
for byte, without rendering anything twice. That is the check a composition split would run on.

**What it buys.** Rendering sits near the top of this list for one reason: it is the only area
whose 1997 code has an automatic, byte-exact oracle. A refactor here is checkable in a way
that a refactor of gameplay or the menu simply is not.

**What it costs.** The constraint is bit-exactness rather than absent coverage, which is a
different and more tractable problem. The equivalence tests define what may not change, so the
work is bounded by a rule that can be checked rather than by judgement. Read
[BIT_EXACTNESS.md](../BIT_EXACTNESS.md) before starting: the three kinds of change it names decide
what is even allowed here.

**A reason has since appeared.** #525: on Windows six corpus saves replay a *different number* of
projection events over a fixed replay, in both directions and across two orders of magnitude. A
different count is a different path through the work, which the `long double` rounding that
explains the other 19 does not account for. That is a diagnosis job needing a Windows machine
rather than a refactor, but it is the first thing to point here.

**Verdict: the composition split is well shaped and still unmotivated; the divergence in #525 is
the thing to look at first.** Splitting OBJECT.CPP along its own seams (render, actor update,
collision) is a real piece of work with a real oracle, but nothing is currently costing anyone
because those four concerns share a file.

### 8. Input bindings: layout against preference

**What.** [SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP)'s `DefKeysDefault95` pairs every action with
an alternate, and that one pair is doing two unrelated jobs. `{K_GRAY_UP, K_NUMPAD_8}` is a layout
fact, one action reachable two ways. `{K_W, K_GRAY_END}` is a preference. Because they share a
slot, rebinding an action silently drops its keypad twin.

The consequence is visible in how the tree reads input. Gameplay asks the binding layer, 164 sites
of `Input &`. The menus, modals and config screens compare raw scancodes instead, about 80 sites
across twelve files, most of them in `GAMEMENU.CPP` (37), `PERSO.CPP` (16) and `CONFIG.CPP` (14).
Every one of those has to remember the keypad legend itself, and #507 records three that did not.

**Coverage today.** The layers either side are host-tested and the one in the middle is not:
`tests/input_device` links `LIB386/SYSTEM/KEYBOARD.CPP` and `MOUSE.CPP`, `tests/input_funnel`
links `LIB386/SYSTEM/INPUT.CPP`, and `tests/menu_keynav` links `SOURCES/MENU_KEYNAV.CPP`, which is
the first piece of the layout layer, added by #506. **`SOURCES/INPUT.CPP`, the binding table
itself, is linked into no test.** Two automation fixtures drive input end to end.

**What it buys.** More open issues than anything else on this list, and one of them is a lock:

- #509, the disc prompt cannot be answered with a gamepad, and a pad-only player sits in
  `while (flag == -1)` with no way out. `CDROM` is unconditional in the default build.
- #508, the Key2 column is unreachable from the keyboard and the keypad period cannot be bound.
- #507 itself, which is the rationale rather than a defect, with #497 and #506 as its evidence.
- #495's remaining third: the gamepad bindings are the settings the provenance work could not
  reach, because they are written by `WriteGamepadConfig` rather than declared in a table.
- #4 and #372, LBA1-style controls and the omnidirectional scheme, both of which are profile
  features and both blocked on this split.

**What it costs.** The most of any area here, and the reason is testability rather than size.
Input is the hardest part of the engine to test after the camera, because most of it only means
anything once a human is holding something down.

The way through is the same one the camera took: find the line CI can see. Which physical keys
carry which intent, and which action a profile puts where, are both pure lookups over a table, so
they can come out as a header with a host test in the way the camera's angle arithmetic did. What
cannot follow is the consumption, and roughly eighty raw-key sites have to be converted a surface
at a time behind the golden captures and the two input fixtures.

**Verdict: do this next.** It is the only area where the issue tracker, rather than this document,
is asking for the work, and the pure part of it is reachable by exactly the pattern that has
already worked twice.

### 9. Gameplay and simulation

**What.** GERELIFE, EXTRA, EXTFUNC, and the roughly 55 gameplay globals.

**Coverage today.** The thinnest of any area: `tests/movement`, `tests/demo_rng_seed`, one
walkthrough fixture. The behaviour is emergent, script-driven and RNG-coupled, and the RNG is a
single shared libc stream, so reproducing a divergence is itself hard.

**What it buys.** In principle the most, since this is where the engine-versus-game line would
eventually be drawn. In practice nothing safely, today.

**Verdict: do not.** Any work here should be adding cover, not moving code. If the goal is the
engine and game split, the LBA1 plan is the cheaper route to it (see below).

---

## The surfaces themselves: config and console

The areas above are features. The cfg reader, the console and the CLI are not, and they need
reading differently, because their size measures something other than their own design.

**Both are already the right shape.** The console core is a library
([SOURCES/CONSOLE/](../../SOURCES/CONSOLE/)) that touches no game state, with `CONSOLE_CMD.CPP`
as the game-bound half compiled separately, which is the split CODESTYLE points at as mature. The
cfg reader has the declarative table. Neither needs restructuring.

**What is left in them is a waiting room.** `CONSOLE_CMD.CPP` still holds 47 commands and names 14
engine globals directly; `BootSettings` still holds 10 rows naming 13. Those numbers do not measure
how badly the surfaces are built. They measure how many *features* have no module to own them:
`teleport`, `varcube`, `vargame`, `behaviour`, `weapon` and `lifetrace` are gameplay, which is area
8, and `Shadow`, `DetailLevel`, `TextureFilter` want a graphics module that does not exist. Audio's
rows and verbs left when audio got one. So both files shrink as a *consequence* of the areas above,
and "refactor the console" is the wrong goal.

Two things are worth doing without waiting for anything:

**Fourteen commands already have an owner.** `playmusic`, `playjingle`, `listjingles` and
`listmusic` to [SOURCES/MUSIC.CPP](../../SOURCES/MUSIC.CPP); `resolution` and `vsync` to
[SOURCES/RES_SWITCH.CPP](../../SOURCES/RES_SWITCH.CPP); `input` and `key` to
[SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP); plus `perftrace`, `distrib`, `credits`, `playvideo`,
`listvideos` and `buildinfo`. Each is the move #568 did for audio, small and independent, and each
shrinks the waiting room now.

**A setting cannot say where its value came from.** This is the one place the surfaces genuinely
interact rather than merely coexist, and the one gap that is theirs rather than a feature's. CLI
beats cfg beats default, implemented through the `stored` and `forced` columns, but
[SOURCES/SETTINGS.H](../../SOURCES/SETTINGS.H) admits the comparison is on the value rather than on
who wrote it: setting a value by hand to what a flag already forced cannot be told from not
touching it. The `on_change` hook added in #569 fires on every runtime write, which is the hook a
writer could record itself through. That would make the flag contract exact instead of heuristic.

## What none of this buys

The obvious claim for restructuring on this scale is that it unblocks something bigger. It does not.

**Hosting LBA1 does not need any of this.** [LBA1_PORT_PLAN.md](LBA1_PORT_PLAN.md) section 0 is
explicit: the work is format transcoders, a script opcode remap and media integration, "not
re-architecting", with the agnostic menu and shell extraction deferred until LBA1 content
actually loads. Do not sell a refactor here as unblocking it.

**None of this makes the game faster.** No area above is motivated by a measurement.

**None of this fixes an open bug**, except indirectly in area 6. The bugs that are open have
their own fixes in flight.

The real buys are narrower: more of the tree reachable from a test CI runs, fewer files that mean
two things at once, and a smaller god header.

## Order

1. **Audio ownership.** Done: #563, #568, #570.
2. **Boot and fatal-error plumbing.** Done: #577. Setting provenance landed with it (#574), which
   is the mechanism #495 asked for.
3. **Input bindings.** Next. The only area the issue tracker is asking for rather than this
   document, and it carries a lock (#509) plus the part of #495 the provenance work could not
   reach.
4. **The game menu.** Largest single payoff, wants a dedicated run at it, and shrinks once input
   stops being read raw in 37 places inside it.
5. **Rendering**, once #525 says what the Windows divergence is.
6. **World and cube**, if the cube-change crash is still open.

Running alongside, in any order and by anyone: de-aggregating one module header from `DEFINES.H`,
and moving one of the fourteen owned console commands. Both are single-sitting jobs that make the
next area cheaper.

The order above changed once the open issues were read against it rather than the other way round,
and that is the better direction. An area this document ranks highly and nothing is asking for is a
worse candidate than one with a lock filed against it, however untidy the first looks.

Running underneath all of it: every one of these should raise the 5% figure at the top. A proposed
refactor that does not raise it needs a different justification.

## How this doc changes

It is expected to be wrong in places and is edited as areas are done, not rewritten at the end.
Three kinds of edit have happened already and all three are the point: an area's cost being
corrected once someone measured it, a prerequisite being discovered that none of the areas had
listed, and a fact being simply wrong.

The wrong one is worth keeping visible rather than quietly fixing, because of how it happened. The
first pass said the ASM equivalence tests do not run in CI, and ranked rendering last on the
strength of it. The evidence was one grep of the platform workflows for a build flag, which found
it off in all three. Those workflows are not where that suite runs. A whole area was mis-ranked by
reading a silence as an answer, and nothing in the checks could have caught it, because a
plan that is confidently wrong still passes every linter.

A finding that generalises past this roadmap graduates out of it. The de-aggregation order above
is a rule about how to do an ownership move at all, not a fact about the audio module, so it
also lives in [CODESTYLE.md](../../CODESTYLE.md) under the ownership rule. When something here
starts reading like a rule rather than a plan, move it there and link back.
