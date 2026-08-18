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

| Scope | Lines | Linked into a host test | Runs in CI |
|---|---|---|---|
| `SOURCES/` | 73,499 | 4,171 (**5%**), 19 files | yes, every push, 3 platforms |
| `LIB386/` | 43,222 | 7,238 (**16%**), 25 files | yes |
| `tests/automation` (59 fixtures) | n/a | n/a | **no workflow references it** |
| ASM equivalence tests | n/a | n/a | **no**, `LBA2_BUILD_ASM_EQUIV_TESTS` is OFF everywhere |

Two things follow, and they shape every section below.

**Every host-tested file in `SOURCES/` is a community TU.** Not one line of 1997 game logic is
reachable from a test CI runs. The 5% is `CLI_ARGS`, `RES_*`, `SAVEGAME_WIRE`, `AUDIO_BALANCE`
and their neighbours, all written in this fork.

**The best coverage in the repo does not run anywhere automatic.** The 59 fixtures drive a real
engine and catch real regressions, but they need retail data and a display, so they run when
someone remembers. The ASM equivalence suite is stronger still and needs Docker and a 32-bit
toolchain. Both are opt-in, which is why an area can be well covered on paper and unguarded in
practice.

Reproduce:

```bash
grep -rhoE "SOURCES/[A-Z_0-9]+\.CPP" tests/*/CMakeLists.txt | sort -u   # what a host test links
grep -rn "LBA2_BUILD_ASM_EQUIV_TESTS" .github/workflows/                # OFF in all of them
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

**Verdict: do this first.** Best ratio in the list, and it ends with a documented statement about
where the gameplay-to-audio coupling actually sits.

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

**Verdict: do this second**, or first if a smaller blast radius matters more than the domain win.

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

**What.** [SOURCES/OBJECT.CPP](../../SOURCES/OBJECT.CPP), 6,787 lines, the only big original file
that has shrunk.

**Coverage today.** On paper the best in the repo: ASM equivalence suites for OBJECT, SVGA, 3D,
ANIM and pol_work, plus `tests/render`, `pol_work`, `texfilter`, `present_rect`. **In practice
none of the equivalence tests run in CI.**

**What it buys.** Nothing yet, and this is the important entry. The prerequisite is not a
refactor: it is getting the equivalence tests to run automatically, or a documented subset of
them. Until then any restructuring here is guarded by a suite someone has to remember to run in
Docker.

**Verdict: blocked, and the block is a CI question rather than a code one.** Raise it separately;
the open work on gating compiler warnings in CI is the nearest neighbour.

### 8. Gameplay and simulation

**What.** GERELIFE, EXTRA, EXTFUNC, and the roughly 55 gameplay globals.

**Coverage today.** The thinnest of any area: `tests/movement`, `tests/demo_rng_seed`, one
walkthrough fixture. The behaviour is emergent, script-driven and RNG-coupled, and the RNG is a
single shared libc stream, so reproducing a divergence is itself hard.

**What it buys.** In principle the most, since this is where the engine-versus-game line would
eventually be drawn. In practice nothing safely, today.

**Verdict: do not.** Any work here should be adding cover, not moving code. If the goal is the
engine and game split, the LBA1 plan is the cheaper route to it (see below).

---

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

1. **Audio ownership.** Small, well covered, proves the pattern generalises.
2. **Boot and fatal-error plumbing.** Small, low risk, makes a failure path testable.
3. **The game menu.** Largest payoff, wants a dedicated run at it.
4. **World and cube**, once #555 lands and if the crash is still open.
5. Everything else is opportunistic, blocked on CI, or better left alone.

Running underneath all of it: every one of these should raise the 5% figure at the top. A proposed
refactor that does not raise it needs a different justification.
