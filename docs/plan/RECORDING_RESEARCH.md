# Recording: research

Where a Doom 3 style recording system should attach in this engine, what it would cost, and
what it buys beyond reproducing input. Written as findings and one recommendation; it commits
nothing and schedules nothing.

Follows [INPUT_DOOM3_RESEARCH.md](INPUT_DOOM3_RESEARCH.md) item E, which named command demos
with a consistency hash as the replay design worth taking, and
[INPUT_REPLAY_RESEARCH.md](INPUT_REPLAY_RESEARCH.md), which established that no recorded input
ships with the game and left the seam, the record unit and the file format open. This doc
answers those three.

Every figure below was measured against the working tree, first with an instrumented build and
then with a working record and replay prototype. The prototype is not committed and is not
proposed for merge; it exists so the design was tested rather than argued. The commands are at
the end. Re-run them rather than trusting a number that has aged.

## The short answer

Record the polled device state at the tail of `UpdateKeyboardState()`, indexed by input poll
rather than by tick, with the clock delta stored beside each sample and an FNV-1a hash of
simulation state stored beside each tick.

That is one function, and the hook that replays it already exists there for another reason. It
covers the whole digital funnel and three analog values sit outside it, priced below.

Record the console commands the session was driven with alongside the input, or a recording
cannot stand in for a harness-driven fixture.

Built and measured. Under a generated clock the design works: sessions replay with no state
mismatch and no clock drift, serially and four at once under load, including the console commands
that drove them. A modal carrying 453 polls inside one tick replays in order, an existing
fixture's trajectory replays four times faster than the fixture runs, and a perturbed replay
reports the tick it stopped matching.

Under a clock sampled from the host, which is what real play uses, replay is not reliably exact.
Extending the proven configuration to a player's own session means generating the clock while
recording rather than reconstructing it afterwards.

## The four layers a recording could sit at

| Layer | Unit | Complete? | Verdict |
|---|---|---|---|
| A. SDL events | `SDL_Event` | no | reject, measured below |
| B. Polled device state | `TabKeys[384]` + `Key` | digital yes, analog no | **take**, plus 20 bytes and the binding table |
| C. Resolved action bits | `Input`, `MyKey` | no | keep as trace, not as record |
| D. Frame-intent struct | a `usercmd_t` equivalent | n/a | the destination, not the start |

### A. An event journal does not work here, and the reason is measurable

Doom 3 inherited Quake 3's `com_journal`: `idEventLoop::GetRealEvent` either calls
`Sys_GetEvent` and writes the struct to disk, or reads the struct back and never calls the
system. It is the cheapest record/replay in that engine because every input the engine has
arrives as an event.

This engine does not consume key events at all.
`UpdateKeyboardState` ([LIB386/SYSTEM/KEYBOARD.CPP](../../LIB386/SYSTEM/KEYBOARD.CPP)) clears
`TabKeys[0..255]` and re-reads `SDL_GetKeyboardState` every poll, and
`HandleEventsKeyboard` receives every key event and keeps none of them beyond setting
`LastInputWasKeyboard`, by an explicit comment. So a journal of key events would not reproduce
`TabKeys` without also reproducing SDL's own key-state machine, which is outside the boundary a
journal can capture.

The second reason is stronger, because it closes the option rather than pricing it. PR #594
measured that this boundary cannot be observed headless at all: there are no SDL key events
without a window. Every fixture in this repo runs headless. A recording layer that is invisible
in the only mode the test suite uses is not a candidate.

### B. The polled state is the digital waist, and it is one function wide

After `ManageKeyboard()` returns, the pair `TabKeys[TABKEYS_NUM_KEYS]` and `Key` determines every
digital input-derived value in the engine. Read from the code rather than assumed:

- `GetInput` ([LIB386/SYSTEM/INPUT.CPP](../../LIB386/SYSTEM/INPUT.CPP), 59 lines) calls
  `ManageKeyboard()`, then rebuilds `Input` from nothing but `CheckKey()` over the combined
  binding table. `NoRepeatInput` is the only other state it reads, and it is engine-internal.
- `CheckKey` is a bit test on `TabKeys`.
- `KeyDown` ([SOURCES/SCAN.CPP](../../SOURCES/SCAN.CPP)) derives its edge set from `TabKeys`
  against its own static previous copy.
- `MyKey` is `Key`, with `JoystickFirstPressedScancode()` as the fallback when no keyboard key
  is down ([SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP)).
- The pad reaches the same table: `GetJoys` writes `TabKeys[256..]`, and
  `UpdateKeyboardState`'s `memset` clears only `[0..255]`, so the pad half survives the
  keyboard rebuild.

`TABKEYS_NUM_KEYS` is `256 + (16 * 8)`, so the waist is 384 bytes plus a scancode.

**The replay hook is already there.** The tail of `UpdateKeyboardState` carries two weak
symbols, `ApplyVirtualKeys` (the touch overlay) and `ApplyHarnessKeys` (the control harness),
whose whole purpose is to write into `TabKeys` at exactly this point. The header comment on the
second one already states the property a replay needs:

> This is the only injection point that a modal loop sees. The `input` timeline ORs its mask
> into `Input` from `MainLoop` (PERSO.CPP), which a menu or dialogue spinning in its own
> `MyGetInput` loop never reaches; every one of those loops does pass through here.

So replay is a third source at a site built for exactly this, and record is one call at the
same site. Every one of the 81 `MyGetInput()` call sites, every modal, every raw scancode
comparison and every `CheckKey` site sees a replayed sample without knowing it is replayed.
That is the id principle, and this engine reaches it without the `usercmd_t` refactor, because
it already has a single funnel where Doom 3 had to build one.

**Three values do not pass through this table**, and a recording that leaves them out reproduces
the keyboard and every digital pad button while quietly dropping both analog cameras:

| Value | Type | Read by | Why it misses the table |
|---|---|---|---|
| `s_firstPressedThisFrame` | `U32` | `MyKey`'s pad fallback, via `JoystickFirstPressedScancode()` | computed in `UpdateJoystick` from `TabKeys` edges against its own previous copy, and `GetJoys` runs *before* `GetInput` reaches the hook, so a replayed table arrives too late |
| `s_rstickX`, `s_rstickY` | `S16` each | the analog camera at [SOURCES/EXTFUNC.CPP:2463](../../SOURCES/EXTFUNC.CPP) | raw SDL axes, never quantised into a scancode when the analog camera owns the stick |
| `MouseXDep`, `MouseYDep`, `Click` | `S32`, `S32`, `U32` | the mouse camera at [SOURCES/EXTFUNC.CPP:1901](../../SOURCES/EXTFUNC.CPP) | `ManageMouse()` is a separate device path with no scancode representation |

The mouse one is not an edge case: `FlagMouseCamera` defaults to `TRUE`
([SOURCES/GLOBAL.CPP](../../SOURCES/GLOBAL.CPP)), so orbiting with the mouse is ordinary play.

The fix is 20 bytes on the sample and a companion restore beside `ApplyHarnessKeys`, not a
different seam. The precedent already exists: the `camnudge` console verb stands in for the stick
and the mouse so a headless run can drive the analog camera, and its comment states that it uses
"the same nudge entry point and same point in the frame as the real sources, so it reproduces
their ordering exactly". A replay wants that property and can reuse that entry point.

### C. The action bits are the wrong record unit and the right trace unit

`Input` plus `MyKey` is 12 bytes against 408, which is the whole attraction. It is incomplete
in three ways that are counted rather than estimated: 93 sites compare a raw scancode outside
INPUT.CPP, 21 sites call `CheckKey` directly, and binding slots 32 to 35 (the four spells) never
reach `Input` at all and are read through `CheckKey(DefKeys[..])` in PERSO.CPP.

It also cannot address a modal. `Input` is ORed in from `MainLoop`, which is why the harness
has both an `input` verb and a `key` verb, and why [CONTROL.md](../CONTROL.md) documents them as
probes at two different boundaries rather than as alternatives.

Keep `input trace` as it is. It answers "what did the simulator inject and what did the hero
do", which is a different question from "what did the device report".

### D. The frame-intent struct is where this lands, not where it starts

[INPUT_DOOM3_RESEARCH.md](INPUT_DOOM3_RESEARCH.md) item F names one small struct saying what the
player asked for this tick, built at one place, as the destination the input increments head toward. A recording
built on B does not compete with that and does not delay it: D is a pure function of B plus the
binding table, so a recording file written today can be re-expressed as a stream of D later
without changing what a recording means.

Building D first would make the recording format wait on the whole input plan. There is no
reason to.

## The index is polls, and the tick seam cannot carry a recording

This is the measurement that decides the format, and it corrects a line in a committed doc.

[CONTROL.md](../CONTROL.md)'s roadmap step 3 says explicit input capture and replay "attaches at
the same top-of-loop seam as `Control_TickHook`". Measured, that seam cannot address the input a
modal consumes.

A counter at the tail of `UpdateKeyboardState`, reported per harness tick:

| Run | Ticks | Polls per tick |
|---|---|---|
| `001 Start.LBA`, `--fixed-dt 16`, 200 ticks | 201 rows | 1 on every row |
| demo reel cube 193, `--fixed-dt 16`, 900 ticks | 900 rows | 1 on every row except tick 877 |

Tick 877 reported **453 polls in a single loop-body iteration**. That is the modal
[INPUT_REPLAY_RESEARCH.md](INPUT_REPLAY_RESEARCH.md) §2e recorded as the reel hanging "after
~876 ticks". A tick-indexed recording has 452 samples with nowhere to put them, and a
tick-indexed replay has 452 polls with nothing to answer them.

Both runs were repeated and the whole poll timeline was byte-identical across the pair,
including the 453. So the poll index is reproducible under `--headless --fixed-dt`, which is
what makes it usable as an index at all.

Two details the format has to account for:

**Polls and `MyGetInput` calls are not the same clock.** The same tick 877 showed 452
`MyGetInput` calls against 453 polls, because `InitWaitNoInput(x)` is `GetInput(x)`, which runs
`ManageKeyboard` too. [CONSOLE.md](../CONSOLE.md) already documents this for the `key` verb.
The poll is the lower and therefore the correct one.

**The clock already advances inside a modal.** `Timer_FixedDtPresent()`
([LIB386/SYSTEM/TIMER.CPP](../../LIB386/SYSTEM/TIMER.CPP)) steps the virtual clock on every
present beyond the one the tick already paid for, which is why a modal terminates under a
virtual clock rather than spinning forever. The fixed-dt machinery already models "a modal spins
N times and the clock moves N-1 steps", so a poll-indexed recording and the existing clock agree
without new machinery.

## Store the clock delta, and variable dt stops being a nondeterminism source

Doom 3 fixed the sample rate (`USERCMD_HZ` 60) so the sampling schedule is a property of the
game rather than of the host, and section 10 of the Doom 3 reading is about what that let id
delete. This engine samples once per rendered frame, so its schedule is a property of the host.
That is the single biggest threat to a recording being replayable, because a recording made on a
fast machine has more samples per second of game time than the same play on a slow one.

The cheap answer is not to fix the rate but to record it. `FixedDt` is a plain variable that
`Timer_FixedDtAdvance()` adds ([LIB386/SYSTEM/TIMER.CPP](../../LIB386/SYSTEM/TIMER.CPP)), so a
recording that stores the elapsed milliseconds beside each sample can drive the same virtual
clock the harness already uses. A recording is then a variable `--fixed-dt`, and the replaying
host's frame rate stops mattering.

This changes what the roadmap assumes. The determinism work concluded that variable per-tick dt
is the dominant nondeterminism source. Recording the dt converts that source from a hazard into
data. It does not close the other two:

- **The audio thread.** With the SDL backend, exterior ambient logic (`GereAmbiance`,
  `IsSamplePlaying`) branches the simulation on wall-clock callbacks, and `SDL_AUDIODRIVER=dummy`
  is not enough; only `-DSOUND_BACKEND=null` removes the path. Recorded in
  [CONTROL.md](../CONTROL.md), not re-measured here.
- **Cross-platform RNG.** One shared libc `rand()` stream, whose sequence and `RAND_MAX` differ
  between glibc and the Windows CRT (issue #530). A recording will not replay across platforms,
  and no format choice fixes that. [SPEEDRUN_MECHANICS.md](../SPEEDRUN_MECHANICS.md) reaches the
  same conclusion from the compatibility-mode direction.

So a recording made in normal windowed play with audio on is not bit-replayable today. The
hash below does not make it replayable. It makes the failure visible and located, which is a
much weaker precondition to satisfy and the reason to build the hash first.

## The recording carries its own oracle, and it is affordable

Doom 3 stores a `consistencyHash` beside every recorded command and stops playback at the first
tic where the replayed hash disagrees, naming the index. The idea transfers directly and this
tree is unusually well placed for it, because `--dump-state` already enumerates the fields worth
hashing.

Measured with an FNV-1a 64-bit digest over the same state `control_dump_state` prints (scene,
hero, camera, inventory, every actor's position and animation, all 256 game vars and all 80 cube
vars), computed once per tick in `Control_TickHook`:

| Property | Result |
|---|---|
| Reproducible | two 600-tick runs produced byte-identical 601-line hash timelines |
| Detects | `--exec-at 300 "input up 60"` diverged first at tick 301, one tick after injection |
| Locates | ticks 301 to 400 differed, then re-converged exactly |
| Cost, 22 actors | 9.6 µs per tick |
| Cost, 5 to 9 actors | 7.0 to 7.5 µs per tick |

Both cost figures are from a `CMAKE_BUILD_TYPE=Debug` build, so they are an upper bound. Against
a 16 ms tick that is under 0.07%. The cost barely tracks actor count because the fixed 336-entry
variable scan dominates the actor loop.

The perturbation is a stronger validation than it looks. `001 Start.LBA` is the house opening,
where the hero is script-owned and input does not move him. The injected walk changed only `anim`, `gen_anim`, `last_frame` and `nb_frames`, and
the hero's `x`, `y` and `z` were identical in both runs at tick 360. The hash caught an
animation-only difference, and the re-convergence at tick 401 is the state genuinely returning to
identical rather than the check going quiet.

The caveat that belongs in the format: a matching hash is evidence, not proof.
State that the hash covers can diverge and re-converge, and state it does not cover can diverge
without ever showing. What the hash buys is the property Doom 3 bought: a replay does not have to
be provably faithful to be useful if it can say when it stopped being faithful.

## What the file has to carry to be self-describing

A recording that only holds samples is a trap, because the same samples resolve differently under
a different setup. The minimum:

| Field | Why |
|---|---|
| Schema version, engine build id | the polyrec precedent (magic + version) and the `--dump-state` `"schema"` field |
| Setup: save name or cube, plus `DemoSlide` | replay has to start where recording did |
| RNG seed | `srand` runs once per cube change, from `Demo_RngSeed(DemoSlide, TimerRefHR, NewCube)` |
| The binding table | see below |
| Per poll: dt, the changed key bits, and the analog values | the sample stream |
| Per tick: the state hash | the oracle |

**The binding table is the non-obvious one.** `Input` is a function of `TabKeys` *and* the
bindings, so a recording of raw key state replayed against a different `lba2.cfg` resolves to
different actions and desyncs for a reason that looks like an engine bug. The recording must pin
the table it was made with.

This is cheap now and was not before: [INPUT_PLAN.md](INPUT_PLAN.md) increment 0 has landed, and
the tables plus the fold live in `INPUT_BINDINGS.{H,CPP}` as a module with host tests rather than
inside 1997 code. Increment 3, named keys, would make the pinned table readable in the file
instead of a block of scancodes. Neither is a blocker; the second is a clear improvement.

**Size.** 384 bytes of `TabKeys`, a 4-byte `Key` and the 20 bytes of analog and pad-edge state
above is 408 bytes per poll raw, and gameplay was measured at exactly one poll per rendered
frame, so a raw stream would be about 24 KB per second at 60 fps. A change-only encoding brings
that down by more than two orders of magnitude: the prototype measured **1.02 to 1.10 bytes per
poll** across two sessions. What actually costs is the per-tick hash, at 13 bytes a tick. The
numbers are in the prototype section. The one part that will not compress is the mouse delta,
which is new on every poll the mouse is moving; that has not been measured under a real mouse.

## The prototype

A working recorder and replayer was built to test the design above rather than argue it. It is
not committed and is not proposed for merge; what follows is what it measured.

**Size of the thing.** 82 added lines across 10 existing files, plus a 779-line module. The
engine side is small because the seam was already there: a call at the tail of
`UpdateKeyboardState` beside the two existing weak hooks, a call at the tail of `ManageMouse`, a
call in `Control_TickHook`, a call at the top of `Console_Execute`, one console verb, and three
accessors for state that had no getter.

**What it does.** `rec start <path>` and `rec stop` record; `rec play <path>` replays;
`rec info [path]` reports the live run's mode, or reads a recording's header and names every line
that differs from the run about to replay it; `rec squeeze <path>` reports what the engine's own
LZSS would give. The file is a text header, a blank line, then a record stream of three kinds:
polls, console commands, and per-tick state hashes. A poll that changed nothing costs one byte.

### What it proved

| Test | Result |
|---|---|
| Record 400 ticks interior, replay | 399 polls, 399 ticks checked, 0 hash and 0 clock mismatches |
| Record real key presses (`key up 120`, `key action 20`), replay with no key command at all | 0 mismatches over 399 ticks |
| Record and replay through the 453-poll modal (demo reel cube 193, 900 ticks) | 1349 polls over 897 ticks, 453 in one tick, 0 mismatches |
| Perturb a replay after the funnel (`input up 30` at tick 300) | consistency failure reported at tick 300 |
| Replay a `--fixed-dt 16` recording under `--fixed-dt 20` | `rec info` named 3 differing lines; clock mismatch at poll 1, hash mismatch at tick 4 |
| Record over the socket, headless | 438 polls captured live across 2.3 s of driving |
| Record over the socket, windowed | works, with the focus caveat below |
| Record and replay the walkthrough fixture's trajectory | 129 ticks, 0 mismatches, reaching cube 49 with vars 164/165 set |

The second row is the design's central claim: a session driven by real key state through the
binding layer was reproduced bit-for-bit from the file alone, with nothing driving the engine but
the recording. The third row is the reason the index is polls: 453 samples landed inside one tick
and replayed in the right order.

### Three findings the prototype produced that the reading did not

**The oracle is the file, not the input.** Measured on two sessions:

| Session | Input stream | Hash stream |
|---|---|---|
| 400 ticks, interior, keyboard-driven | 439 bytes over 399 polls (1.10 B/poll) | 5,187 bytes over 399 ticks (13 B/tick) |
| 900 ticks, demo reel with the modal | 1,381 bytes over 1,349 polls (1.02 B/poll) | 11,661 bytes over 897 ticks (13 B/tick) |

So the change-only encoding works: recording what the player did costs about a byte per poll, and
the per-tick state hash costs thirteen times more. At 60 Hz that is roughly 61 bytes a second of
input against 780 of oracle, so an hour of play is about 220 KB of input and 2.7 MB of hash. The
lever is the oracle, not the samples, and the compression section below measures how far pulling
it goes: a 32-bit digest every tenth tick took the same session from 13,396 bytes to 2,545, and
LZSS then took the payload to 932. The text header was 319 bytes. A real play session, measured
later, reorders this: capturing the clock at every `ManageTime` call costs more than the oracle
and the input together.

**Replay is authoritative at the funnel, and that is a boundary worth knowing.** Injecting
`key action` into a running replay changed nothing, because `ApplyHarnessKeys` runs before the
replay hook and the hook overwrites the whole table. A replay therefore cannot be perturbed by
live input, the touch overlay, or a real keyboard, which is the right default for a fixture and
means nudging a replay mid-flight needs an explicit takeover rather than just pressing a key. The
authority stops at the funnel: `input`, which ORs into `Input` from `MainLoop`, does perturb a
replay, which is how the oracle row above was produced.

**The recorded clock is a faster detector than the state hash.** Replaying a 16 ms recording at
20 ms was caught at poll 1 by the clock delta and only at tick 4 by the hash. Comparing one
recorded integer per poll costs nothing and fires before any state has had time to drift, so the
clock check earns its place beside the hash rather than being redundant with it.

### The mode header, and what it is for

`rec info` against a mismatched run named exactly the lines that differed:

```
rec: MISMATCH mode.fixed_dt=16
rec: MISMATCH mode.resolution=640x480
rec: MISMATCH clock.timer_ref_hr=4268109
rec: 3 mode line(s) differ; replay may not reproduce
```

The header the prototype writes is engine version, headless, fixed dt, fixed timestep, vsync,
build flags (which carry the sound backend, the one that decides exterior determinism),
resolution, language, island, cube, `DemoSlide`, the clock at start, and a digest of the binding
table. That last one matters for the reason [INPUT_PLAN.md](INPUT_PLAN.md) increment 0 makes it
cheap: `Input` is a function of the key table *and* the bindings, so the same samples under a
different `lba2.cfg` resolve to different actions.

Reporting the mode is most of what makes a recording safe to accept from someone else. A bug
report that replays differently should say which of thirteen lines differ before it says
anything about the game.

### The socket drives with a head and without

Both work, and the windowed case needed one change. `Timer_SetIgnoreFocus(1)` was armed only for
a run that is headless, has a tick budget, or exits on its own. A `--listen` run has none of
those, so a windowed session froze the moment the operator moved focus to the terminal they were
driving it from, which is the only place they could be. Measured, with focus changed by hand
during the run:

| Windowed `--listen` run | Game clock over 3 s of wall time |
|---|---|
| focus loss handled as for a player | 0 ms |
| focus loss ignored | 3,011 ms |

`SDL_EVENT_WINDOW_FOCUS_LOST` calls `LockTimer()`
([LIB386/SYSTEM/TIMER.CPP](../../LIB386/SYSTEM/TIMER.CPP)), which is right for a player and wrong
for a driven session. Three ways to close it, and the prototype implements the first two:
`--listen` implies ignoring focus, since a socket-driven run has a driver and the driver is in
another window by definition; an explicit `--ignore-focus` for the general windowed case; or a
cfg key for anyone who wants it always. Inferring it from `--listen` is the one that needs no
documentation and has no case where it is wrong, so the flag is for everything else.

Driving with a head matters for this specific feature in a way it does not for the rest of the
harness, because watching a replay is how a person judges whether it reproduced something a hash
does not cover.

### Console commands belong in the recording

A real fixture exposes a gap the reading missed.
[test_walkthrough_opening.sh](../../tests/automation/test_walkthrough_opening.sh) drives the
opening of the game with `teleport`, which is a console command, not a key. No amount of recorded
input brings that back, so a recording of input alone desyncs on the first command.

Capturing at `Console_Execute` closes it: a command record carries the tick it was issued on, and
replay runs it at the same point in the tick. That makes a recording a superset of the input
stream rather than only the input stream, and it is what lets one replay stand in for a
harness-driven session.

### Recording a real fixture

The walkthrough's trajectory, recorded once and replayed with nothing driving the engine:

| | Result |
|---|---|
| Replay | 129 ticks checked, 0 hash and 0 clock mismatches, 2 commands replayed from the file |
| Reached | cube 49, game vars 164 and 165 set, which are the fixture's own beat 3 and beat 4 assertions |
| Fixture as written | 3.86 s, five cold boots |
| Same beats as one replay | 0.94 s, one boot |

Four times faster, and the assertion comes with the file instead of being maintained beside it.

**Two things it does not replace.** A recording collapses a *sequence* into one
run; it cannot collapse a *comparison*. The walkthrough's beat 2 asserts that held input moves the
hero not at all during the house opening, which is two runs diffed against each other, and no
single timeline expresses it. And the fixture fails with a sentence: "did not transition through
the door: cube=0 (want 49)". A recording fails with a tick number. The recording is the better
regression net and the worse diagnosis, so the two belong together rather than one replacing the
other.

### Compression is the wrong first lever, then the right second one

The engine ships LZSS ([SOURCES/LZSS.CPP](../../SOURCES/LZSS.CPP)), used for savegames and the HQR
containers, so the obvious answer to file size was to reuse it. Measured on three recordings, it
recovers almost nothing:

| Recording | Payload | After LZSS |
|---|---|---|
| walkthrough, 129 ticks | 1,864 B | 1,828 B (98.0%) |
| interior, 399 ticks | 5,626 B | 5,172 B (91.9%) |
| demo reel, 897 ticks | 13,042 B | 10,245 B (78.5%) |

The reason is structural rather than a property of LZSS: eight of every thirteen bytes in a tick
record are a digest, and a digest is incompressible by construction. Compressing a file that is
mostly hash cannot work, whatever the coder.

Thinning the oracle does work, and then compression starts working too. The same 900-tick demo
reel session, recorded four ways:

| Oracle | File |
|---|---|
| 64-bit digest, every tick | 13,396 B |
| 32-bit digest, every tick | 9,808 B |
| 64-bit digest, every 10th tick | 2,905 B |
| 32-bit digest, every 10th tick | 2,545 B |

LZSS on that last one takes its 2,191-byte payload to 932 bytes, 42.5%, because what is left is
structured rather than random. End to end that is 13,396 bytes down to about 1,300.

**What thinning costs is precision, and only precision.** A `teleport` perturbation at tick 400
was reported at recorded tick 398 by the every-tick oracle and at 400 by the every-tenth one:
located to within the hash interval instead of exactly. Both caught it. So the interval is a dial
between file size and how tightly a failure is pinned, which is a better thing to have than a
compressor.

**One control, and it changes the reading of the row above.** The first perturbation tried
against the thinned oracle was `input up 30`, and it was not caught. It was not caught by the full
oracle either: under `DemoSlide` the hero is script-driven and injected input
changes nothing the hash covers. A perturbation that changes nothing is invisible at any digest
width, which is the "evidence, not proof" caveat above arriving as a measurement rather than a
caution.

### Metadata the fixtures argued for

Two lines were added to the header after this pass. `data.master` and `data.distrib`, from
`Distrib_GetMaster()` and `Distrib_Name()`, because a recording made against one data lineage
replayed against another diverges for a reason that is not the engine, and
[the RESS fingerprint](../VERSIONS.md) is what distinguishes them. The header now runs to fifteen
lines.

### A real play session

A recorded session of ordinary windowed play with audio, 47 seconds of game time, is what the
headless measurements above could not stand in for.

| | |
|---|---|
| Polls | 242,280 |
| Ticks | 17,219 |
| ManageTime clock samples | 5,464,135 |
| File | 6.1 MB |

**The clock stream is 90% of the file, and capturing it where the engine reads it is why.**
`ManageTime` was called **22.6 times per input poll**. Headless the same figure is 1.0, so every
size estimate taken headless understates a real session by more than twenty times.

99.4% of those samples are a zero delta, because the calls inside one frame read the same
millisecond. Run-length encoding just the zeros takes the clock stream from 5.46 MB to about
89 KB and the file to roughly 700 KB, which is a bigger win than the digest thinning above and
does not cost precision. It is the first thing to do to this format.

**Interleaving the clock samples with the poll samples was a mistake.** One `ManageTime` call more
or less on replay than on record leaves a clock record where a poll is expected, and the reader
had no way through it. Consuming the surplus and counting it, rather than stopping, is a patch;
the fix is a clock stream that is addressed independently so a difference in call count is a
reported divergence instead of a parse failure.

**What the session replayed.** Headless, against a recording made windowed with audio:

- the whole menu portion, 76,587 polls, with **no hash mismatch**
- then a consistency failure at tick 2 of the first scene, with 582 surplus and 387 deficit clock
  samples across the transition

So the menu reproduces and the first scene load does not. Whether that is the environment
(windowed with a live audio thread, replayed headless without one) or the format is not yet
separated: a replay with audio alive could not be run here, because SDL audio initialisation
hangs in a headless-with-audio run on this machine. That is the next experiment and it is the one
that decides open question 2.

**Two things the session confirmed that were argued for rather than measured.** The player's
`bindings.digest` differed from the shipped default, so a recording replayed against default
bindings really would resolve different actions, which is what the field exists to catch. And the
session used no analog input at all: 0 polls carried mouse or stick data, so the 20 analog bytes
are free for a keyboard player and only a mouse-camera player pays them.

**The input itself stays almost free.** 1.32 bytes per poll, 320 KB across the whole session,
against 5.5 MB of clock. Recording a player is cheap; recording time is not.

### The recorder has to be passive, and proving it is a test

Pinning the game clock to one sample per input poll at record time arms the harness virtual clock
on a normal player session, where it stops input responding at the boot banner, which is how far
a recorder can be from passive while still looking like it works.

The measurement that settles it is one A/B: run the same scripted session with and without the
recorder and diff the state. Pinned, hero animation state differs. With the capture inside
`ManageTime`, every field is identical.

That test belongs beside the format, not in a commit message. A recorder that perturbs the run it
observes produces recordings of a game nobody played.

### A generated clock replays; a sampled one does not

This is the result the rest of the section is in service of, and it separates cleanly.

**Generated.** Recorded and replayed under `--fixed-dt`, where the clock is produced by the
engine rather than read from the host:

| | First hash mismatch | Clock drift |
|---|---|---|
| 4 sessions, run serially | none | 0 ms |
| the same file replayed 4 times at once | none | 0 ms |

Exact, and exact under load, including the console commands that drove the sessions. That is the
viable configuration and it is enough to say the design works.

**Sampled.** Recorded in real time, where the clock is read from the host, replay is *not*
reliably exact. Three runs early on came back clean and a later four did not, on the same code:
the difference is machine load, because the replay only reproduces the recording while it makes
the same number of `ManageTime` calls, and load changes that. Any exactness claim for the sampled
path in this document that is not on this line was measured on a quiet machine and does not
generalise.

The two differ because a generated clock is a property of the session and a sampled one is a
property of the host, which is Doom 3's `USERCMD_HZ` argument arriving from the other direction.

### The clock stream is positional, and that is why the sampled path fails

Capturing the clock at `ManageTime` is right in one respect: it is where the engine reads it, and
it keeps the recorder passive.

Writing those samples inline in the same stream as the polls is wrong. The stream is then
positional, so it holds only while the replay makes exactly as many `ManageTime` calls as the
recording did. Two things break that immediately:

- **A recorded console command**, on the sampled path. Running one changes control flow, and
  therefore the call count. A recording carrying a `teleport` stops at poll 3 with the streams
  parted. Under a generated clock the same recording replays exactly, because the call count no
  longer decides anything.
- **A different environment.** A session recorded windowed with audio and replayed headless
  parts at the first scene load, with 582 surplus and 387 deficit samples across the transition.

Consuming the surplus to keep going is worse than stopping. It trades exactness for the
appearance of working: the clock the simulation then reads is not the recorded one, and the hash
starts failing several ticks later for a reason that no longer points at the cause. Measured, the
tolerant reader turned three exact replays into three that failed at tick 8.

**The fix is to stop making the clock positional**, and the size measurement points the same way.
99.4% of clock samples are a zero delta, because every call inside one frame reads the same
millisecond, so one sample per poll collapses 5.46 million records into the 242 thousand the poll
stream already carries.

Carrying one sample per poll was built and measured. It is robust, it is passive, and it is not
exact: the 0.6% of calls that do move the clock inside a frame are lost, and the hash starts
failing around tick 8. Quantising at record time as well, so the recording and the replay agree
by construction, removes the clock drift entirely but leaves a residual the run did not close.

Quantising the sampled clock at record time was then built, so that recording and replay agree by
construction. It closes the clock question and not the whole one:

| Between record and replay | Result |
|---|---|
| Game clock at every tick | identical, 0 ms drift |
| Polls, ticks, polls per tick | identical: 40, 40, 1 |
| Recorded input | identical by construction |
| State hash | first mismatch at tick 6 |
| The whole difference at that point | `actors[12].anim`, 221 against 220 |

Chasing that field is what produced the answer. Over longer runs the divergence is not one
animation but a moving actor: `actors[5]` drifting in `x`, `y`, `z` and `beta` together, which is
the signature the determinism work already recorded for an unpinned dt, where "the two actors that
were actively walking" were the only things that moved between runs.

**So a reconstructed clock is not enough, and cannot be made enough.** The engine's determinism
does not need a clock that is *reproduced*, it needs one that is *constant*: movement is
integrated per sub-step from `GetDeltaMove`, and pinning the value at tick boundaries leaves the
integration free. Reproducing the source, pinning the derived game clock at every tick, and
deferring the baseline restore past the clock arm were each built and each measured, and none of
them closes it, because none of them changes that.

That is why `--fixed-dt` exists, and it is the same conclusion from the other side: **a session
that is to be replayed has to be recorded on a fixed timestep.** Doom 3 reached this by fixing
`USERCMD_HZ` and deleting the frame-time arithmetic that a variable rate had needed; here the
equivalent is that recording pins the timestep rather than trying to capture a loose one.

That is not a limitation to work around, it is what recording means. A recording is a session
somebody intends to reproduce, so quantising its time is inherent to the act rather than a cost
imposed on it, and the engine already has a fixed-timestep simulation for
[movement frame-rate independence](../MOVEMENT_FRAMERATE.md).

**Two things a recording mode has to get right, both measured.**

*Pace the frames.* A pinned step alone makes game speed a function of frame rate, because there
is no frame limiter in the engine and only vsync paces the loop: 600 ticks at a 16 ms step is
9,600 ms of game time and took 3.53 s of wall time. Holding each frame to the step brings that
back to real time, measured at 4,800 ms of game time in 4.85 s. Without it a recording session is
unplayable on any machine whose frame rate is not exactly the step.

*Arm the step where the determinism work already established it belongs.* `Timer_EnableFixedDt`
is called at the top of `Control_Begin`, before `InitGame` and `ChangeCube`, so that boot's own
`ManageTime` calls freeze and the base clock is the restored save value exactly. Arming anywhere
else was tried, both at the first input poll and immediately after the command line is parsed, and
neither reproduces: boot wall-clock leaks into the base, and the frames either side of the
handover run on different clocks, which parts a recording from its replay within a couple of
ticks. Under the established arming point the same test is clean four times out of four with the
final state identical.

So `--record` should route through that arming path rather than introduce another. Passing
`--fixed-dt` alongside it is what does that today, and recording now paces its frames to the step
whenever both are on, which is what makes such a session playable rather than merely reproducible.

### Mid-session recordings, and the snapshot that makes them replayable

A recording that starts mid-session has no setup, so replaying it would need everything before it
reproduced first. The engine already writes the setup: a savegame.

**How faithful a save is as a snapshot, measured.** Take one mid-session, load it back, and diff
against the state at the moment it was taken: **six fields differ**. One is the clock. The other
five are the x, z and beta of the two actors that were moving, which are re-derived from their
scripts on load rather than restored. Everything else, the hero, the scene, every quest variable,
the inventory and every static actor, comes back exactly.

So `rec start` writes a snapshot beside the recording and names it in the header, and a
mid-session recording replays through the proven `--load` path:

```bash
lba2cc --load session.rec.lba --fixed-dt 16 --replay session.rec
```

**What that reproduces, over a 200-tick mid-session replay:** the hero identical, the scene
identical, every quest variable identical, and ten fields adrift, all of them the moving actors
plus a debug counter. The hash reports the divergence at tick 0, immediately, because the replay
starts from the post-load state while the recording continued from the live one.

Closing that last gap is one change rather than a new mechanism: `rec start` should load its own
snapshot back, so the recording begins from the same post-load state a replay will. The cost is a
scene reload where recording starts, which is a visible hitch for something a person asked for
deliberately.

**The savegame is sufficient. What matters is that both ends take the same load path.**

A save restores the scene, the hero and the variables, and actor kinematics are re-derived on
load. So the state a load lands in depends on which load path ran, and two measurements bound it:

| Compared | Fields differing |
|---|---|
| A mid-session reload against a boot load of the same file | 5, the x, z and beta of the two moving actors |
| The same file loaded mid-session at tick 100 and at tick 200 | **0** |

The second is the one that matters. **A mid-session load is independent of what preceded it**, so
it is a reproducible starting point even though it is not the same one a boot load reaches. The
route to a mid-session recording that replays exactly is therefore for both ends to go through it:
the recording loads its own snapshot back before the first recorded frame, and the replay loads the
same snapshot the same way before the first replayed one. Neither end uses `--load` for that,
because `--load` is the other path.

That is setup and teardown belonging to the replay itself rather than to the command line, which is
also what makes it composable with the console's own `load` and `cube` verbs: a recording's setup
is a thing the engine can perform, not a thing the caller has to remember.

**Built, and landed.** `setup.reloaded` is the header line that keeps the two ways in apart: a
mid-session `rec start` writes its snapshot, reloads it, and records from the first post-load frame;
a from-boot recording reloads nothing and says so, and a replay reloads only when the recording did.
Both ends go through the tick hook, which is the one place in the loop body that runs after
`ChangeCube`. Measured, on a 400-tick session from the same save:

| Started by | Replays |
|---|---|
| `--record` / `--replay`, from boot | clean, three runs out of three |
| `rec start` / `rec play`, mid-session | clean, three runs out of three |
| `rec start`, replayed through `--replay` | clean, because the flag is read rather than assumed |

**Two things had to be true before the reload reproduced, and neither is about the load.** Both
showed as the same symptom -- identical state at tick 0, divergence at tick 1 -- which is what a
setup difference looks like when the setup is a savegame.

The first is the clock. The header's baseline is applied on replay through `SetTimerHR`, whose
`LockTimer` banks the frame's pending delta one line before the assignment throws it away. Sampling
the baseline before that bank leaves the replay one frame of clock behind. From boot the delta is
zero, because the first recorded poll is the run's first poll, so the flag path never showed it;
after a reload it is one step, and it was worth 16 ms of drift. Arming the frame clock and letting
`ManageTime` settle it before the header is written closes it, and changes nothing from boot.

The second is the clock the load itself is taken at, and it accounts for the whole of the rest.
The two ends do not reach the load with the same reading, because writing the snapshot costs the
recording a frame the replay never spends: measured, `TimerRefHR` 6798097 against 6798113, one step
apart. A savegame restores the fields the state hash covers and re-derives the rest, and some of
what it re-derives is stamped from that reading, so the two ends come out of the load looking
identical and drift later. `setup.reload_clock` carries it, and the replay sets the clock there
before issuing its own load.

That subsumes the RNG. `ChangeCube` is the engine's only `srand` and outside the demo reel it seeds
from `TimerRefHR` ([SOURCES/OBJECT.CPP](../../SOURCES/OBJECT.CPP)), so two ends reaching the cube
change at different readings draw different streams. With the clock pinned before the load both
reach it at the same `TimerRefHR` and the seed follows without being carried, which settles it at
the cause rather than the symptom, and it holds for cube changes later in a session where a seed
named in the header could not reach. Verified by removing the reseed: both scenes stay clean.

**What this cost in scope.** One header flag was the plan and two lines is the outcome, because a
mid-session reload sets up a clock as well as a scene and nothing else in the file carried it.

**Two limits, both measured, both to solve later.**

*Closed: a mid-session recording in an exterior scene.* It diverged at tick 296 of 397, three runs
out of three, with the clock in step and the stream in step. How it was localised is worth keeping,
because a digest says when and never what: dumping named state either side of the tick and diffing
came back with **one field**, actor 8's `beta`, 758 against 512, on an actor that never moves. A
tick-by-tick trace then showed the recording turning it from tick 299 and the replay never turning
it at all, with its Life script tracing identically at both ends, which ruled out the script and
left the state the load had stamped. The cause is the reload clock above, and pinning it closes it.
All four combinations replay clean three runs out of three:

| Path | Scene | Result |
|---|---|---|
| `--record` / `--replay` | interior (cube 3) | clean, 401 ticks |
| `--record` / `--replay` | exterior (cube 48, Downtown) | clean, 401 ticks |
| `rec start` / `rec play` | interior (cube 3) | clean, 397 ticks |
| `rec start` / `rec play` | exterior (cube 48, Downtown) | clean, 397 ticks |

**A trap worth not repeating.** The exterior flag-path run that first read as "clean" stopped at
tick 300, and the divergence is at 299. A control that ends where the fault begins is not a
control, and the flag path had to be re-run to 401 ticks before the comparison meant anything.

*Closed: recording could not survive a scene fade.* `Record_ClockHook` held `ManageTime` at the
last input poll, and `FadeToPalAndSamples` ([SOURCES/AMBIANCE.CPP](../../SOURCES/AMBIANCE.CPP))
loops on `ManageTime` plus `Timer_FixedDtPump` until the fade reaches `FADE_DELAY` and never polls
input, so the fade never completed and the loop never ended. The same run finished without
`--record` and hung with it, which is what named the recorder rather than the scene.

The fix is to stop quantising under a generated clock. `--fixed-dt` already produces one value per
tick and steps it deliberately in exactly the places that wait without polling, so the pin has
nothing to add there and only takes the pump away. Under a clock sampled from the host there is no
pump and the quantisation is what makes a replay possible at all, so it stays. That is a one-line
exception, and it buys two things:

| | Before | After |
|---|---|---|
| `--record` from boot, no `--load` | hangs at the first fade after the menus | records, and **replays clean three runs out of three** |
| a recording containing a cube change | hangs | records and replays, and diverges at the change |

The first is the bigger of the two: recording from boot is the case a player's own session takes,
and it could not be captured at all.

*Closed, and it was not the clock.* With the fade fixed, a cube change at tick 40 still failed at
tick 41 with 656 ms of drift, which reads exactly like a clock problem and is not one. Probing the
cube number at both ends says why in one line: **the recording changed cube and the replay never
did.** The drift is the consequence, since one end spent a scene load the other never performed.

The cause is that a replay executes its recorded console commands through `Console_Execute`, and a
run driven only by `--replay` has never registered the console's command table: the harness calls
`Console_EnsureRegistered` before its own `--exec` and the recorder did not. Every recorded command
therefore dispatched to nothing, silently, on that path. It is not specific to `cube`; the reason it
had not been noticed is that the fixture work that established commands replay at all drove the
replay with `--exec "rec play"`, which registers the table on the way in.

Recording the console commands is what makes a recording a superset of the input stream, so this had
quietly removed the property that section claims. With the registration done where the replay
begins, a recording that crosses a cube change replays clean three runs out of three with no drift,
by either way in, and **a real zone-triggered transition does too**: walking out of Downtown crosses
from cube 48 to 42 at tick 788 on its own, and that session replays clean over 901 ticks.

### How the game changes cube, and what the save has to do with it

Two of the three faults above live in this window, and a third route through it is still untested.

Every route sets `NewCube` and lands at the same `ChangeCube`, but they are not equivalent, and
`FlagChgCube` is what separates them:

| Route | `FlagChgCube` | Where |
|---|---|---|
| a zone the hero walks into | 1, position taken relative to the zone | [SOURCES/OBJECT.CPP](../../SOURCES/OBJECT.CPP) |
| walking off the island map | 1, into the phantom cube | [SOURCES/EXTFUNC.CPP](../../SOURCES/EXTFUNC.CPP) |
| a Life script instruction | 2, positioned on the saved start | [SOURCES/GERELIFE.CPP](../../SOURCES/GERELIFE.CPP) |
| the `cube` console verb | **0** | [SOURCES/CONSOLE/CONSOLE_CMD.CPP](../../SOURCES/CONSOLE/CONSOLE_CMD.CPP) |

`ChangeCube` runs `SaveTimer()` only when the flag is set, and branches on it again for the camera
and the start position, so the console verb is the one route that skips the timer lock. A test built
on it is testing the quiet path. That is why the real transition above was recorded as well.

**The savegame carries the clock, and that is the detail the reload fix turns on.**
`SaveContexte(savetimerrefhr)` writes `TimerRefHR` into the file and `LoadGame` restores it with
`SetTimerHR` ([SOURCES/SAVEGAME.CPP](../../SOURCES/SAVEGAME.CPP)), so both ends of a reload come out
of the load holding the same game clock. The gap is upstream of that: `ChangeCube` seeds the RNG
from `TimerRefHR` *before* it reaches `LoadGame`, so the seed and anything else stamped in that
window come from the clock the run walked in with, not the one the file restores. That window is
exactly what `setup.reload_clock` closes, and it is why the save carrying a clock did not close it
by itself.

Also in that window: `SaveValidePos` is skipped while `FlagChgCube` is set, on the comment "on ne
sauve pas le contexte", so a transition deliberately does not checkpoint mid-flight.

**Two things about `FlagLoadGame` still decide any future attempt, and both are easy to get
wrong.**

It is cleared at `MainLoop`'s `restartloop` label ([SOURCES/PERSO.CPP:460](../../SOURCES/PERSO.CPP)),
which the loop passes through on entry and on a restart, and not once per iteration. `InitGame(-1)`
sets it, and the load itself lands at the next `ChangeCube`
([SOURCES/OBJECT.CPP:1374](../../SOURCES/OBJECT.CPP)). So waiting for the flag to clear before
beginning a recording never fires: a mid-session load leaves it set, because nothing on that path
returns to the label. The signal to wait on is the cube change having happened, and the tick hook
already runs after it in the loop body.

And it stays set, where a fresh `--load` reaches `MainLoop` and clears it. A recording made after a
mid-session reload would therefore run with the flag set while its replay runs with it clear, and
several places in the object loop branch on it. Clearing it as part of the reload is what makes the
two match, and is the part to get right rather than the load call.

### A snapshot at both ends

The same mechanics at `rec stop` give a recording a second oracle, and the two answer different
questions. The per-tick hash says *which tick* a replay stopped matching. The end snapshot says
*what it ended up with*, in named fields rather than a digest, so a replay's final state can be
diffed against it directly. Verified: the end snapshot's hero position, animation, scene and quest
variables match the state at the moment recording stopped.

It is written whenever the recording is stopped, including through a clean shutdown, and a scene
has to be live for it to mean anything. **A hard crash leaves it absent, and that is the right
outcome rather than a gap to paper over:** its absence says the session did not finish, which is
exactly what a crash repro wants to record. Confirmed by killing a recording run outright, which
leaves the recording and its opening snapshot intact, per-tick flushed, and no end snapshot.

### The two ways in are not equivalent, and only one reproduces

The flag and the console verb start a recording at different points, and it shows:

| Started by | Replays |
|---|---|
| `--record` / `--replay` | clean, three runs out of three |
| `rec start` / `rec play` on the first tick, same pinned step | fails at tick 41, three runs out of three |

The console failure is at the same tick every time, which is the release edge of a key held across
it, so this is a systematic offset rather than noise. A console command runs from the tick hook,
which is after that tick's input poll, so a verb-started recording begins a poll later in the
frame than the flag does; arming the verb to the next poll boundary was tried and does not close
it, so the offset is not the whole story.

**What that means in practice.** `--record` is for anything that has to replay. `rec start` is for
capturing a session to look at: what was pressed, how long the frames were, where the modals sat,
what the recording would cost. Both produce a file the inspector reads; only one produces a file
the engine can play back faithfully. `rec start` now says so when the step is not pinned at all,
which is the case where nothing about the recording could reproduce.

### Where this points the harness

The socket is the better driving mechanism, and building the recorder is what made that clear
rather than being a separate opinion about it.

`--exec-at` needs the whole plan before boot, so anything whose next step depends on what the last
one showed costs a boot per observation: **282 ms against 0.6 to 1.2 ms** over the socket, on the
figures [CONTROL.md](../CONTROL.md) already carries. It is also serviced from the pre-present
chain, which every mode reaches, where `Control_TickHook` has a single call site inside `MainLoop`
and is dark for the whole of a menu or a cinematic. Every awkward moment in building this recorder
was that same gap: the tick hook cannot see a menu, so the recorder had to start from the input
poll instead; and the reload that would make a mid-session recording exact fails precisely because
it is driven from a point in the loop that cannot finish it.

The objection to the socket has been that a session driven over it does not replay, which
[CONTROL.md](../CONTROL.md) states as a property: commands arrive at whatever tick the driver sent
them. **A recorder removes that objection in principle**, because it captures those commands with
the tick they landed on, so an exploratory session becomes a file that replays. Driving with the
socket and recording what happened is a better division than choosing between reacting and
reproducing.

**In practice it does not work yet, and the reason is not the commands.** A session driven over the
socket, recorded and replayed from its own snapshot, fails. So does one that carried no command at
all, with a nonsense tick number, which is the reader misaligned rather than the simulation
diverging. Meanwhile a recording started and replayed through the console verbs, carrying the same
`key` command, is clean: 299 polls and 299 ticks on both sides, 299 ticks checked, no mismatch. So
commands through the bus reproduce, and something else about a socket session does not.

**The socket is not the variable. Where the recording starts is.** Every socket session tested
had its recording started mid-session over the socket, and mid-session is the case that already
had a known cost. Controlled for: a mid-session recording started from the console, on a build
with the socket compiled out entirely, fails at tick 0 in both runs, while a from-boot recording
on that same build is clean. So the failures attributed to the socket are the mid-session snapshot
gap, and nothing here demonstrates a problem with the socket at all.

That gap is the one measured earlier: a replay loads the snapshot and starts from the post-load
state, while the recording carried on from the live one, and the two differ in exactly the actors
that were moving. Tick 0 is when the hash first looks, so tick 0 is where it says so.

That makes the fix the same fix: records that are addressed rather than positional.

**Sync markers are that fix, on the socket protocol's own idea.** The protocol terminates a
response with `<<END>>` so a driver that has lost its place has something to find again, and a
recording needs the same. One marker every 64 polls carries the poll and the tick the recording
was on, at about 0.2 bytes a poll. A reader checks each against its own counters, says once where
the two parted, and adopts the file's; a reader that meets a record it cannot parse scans forward
for the next marker and picks the stream up there.

What that changes for the socket case: a session that previously reported a consistency failure at
tick 4,248,410,325, which was the reader lost in whatever the bytes happened to say, now reports
that it resynced at the marker for poll 64 and ended at poll 75, the file's real length. The
session still diverges, and the presents are still why, but the difference between a diagnosis and
a nonsense number is the difference between a next step and a shrug.

Both readers have to step over a marker, because one lands between a poll and the tick record that
follows it and whichever reads next meets it first. Handling it in the poll reader alone silently
put the tick stream one record behind, which read as a consistency failure at tick 10,706,741 on a
path that had been clean.

### The recorder does not depend on the socket

The socket is a debug-build-only feature and recording is not. `RECORD.CPP`
names nothing from the control server, and with `LBA2_CONTROL_SERVER=OFF` a session recorded and
replayed through the `rec` console verbs is clean three runs out of three with the final state
identical. The socket is something a recording works well alongside, not something it needs.

**A correction, since an earlier reading of this is committed.** The console verb was recorded here
as failing at tick 41 where the flag path was clean, and the blocking item was written up as
commands not replaying. Re-measured after the snapshot work landed, the console path replays clean
and the flag path does too. Whatever the tick-41 failure was, it is gone, and the surviving
difference is the socket rather than the verb.

### What the prototype does not do

Replay anything whose control flow differs from the recording's, for the reason above. Input-only
sessions in a matching environment replay exactly; a recorded console command or a changed
environment does not.

Carry the clock as one sample per poll rather than one per `ManageTime` call, which is the single
change that would fix both the fragility and the file size.

Verify anything in the menus. The state hash is written from `Control_TickHook`, which only runs
in `MainLoop`, so a 25-second menu recording carries 19,736 polls and zero ticks. Menu navigation
records and replays; nothing checks it. The existing UI capture goldens are the oracle that
would.

It also carries the mouse at the keyboard poll index rather than at `ManageMouse`'s own, so the
mouse delta a sample holds is the one drained on the previous frame. Consistent between record
and replay, and wrong if anything ever reads the two at different rates. A real format would give
the mouse its own index or move the drain.

## Settings, and what a recording owes them

A recording reproduces the input and the savegame reproduces the game state. The cfg is the third
input to a run and the file carries almost none of it, so this section measures which settings a
replay is actually sensitive to, and settles what the recording should do about them.

**The recommendation, first.** Record the settings as metadata and do not apply them on replay.
Record only what differs from its default, so a cfg key added later does not invalidate a file
written before it existed, and so the line stays short on the overwhelmingly common run where the
player changed two things. A replay that lands differently can then be diffed against what it was
recorded under, one setting at a time, as the need arises. Two exceptions are named below, and the
input bindings are the one that has to be acted on rather than reported.

### Which settings actually break a replay

Measured on a clean baseline: an exterior recording (Downtown, cube 48, auto camera on, hero
walking 150 ticks), `--record` then `--replay`, no mismatch over 301 ticks. Each row changes one
cfg key on the replay side only:

| Setting | Changed | First hash mismatch |
|---|---|---|
| `FollowCamera` (Auto camera) | 1 to 0 | **tick 0** |
| `DetailLevel` | 3 to 0 | **tick 235** |
| `Input0_1` (rebind forward) | 82 to 26 | **tick 33** |
| `FollowCamDriftDivisor` | 24 to 8 | none |
| `FollowCamOrbitGlide` | 75 to 20 | none |
| `FollowCamGroundClearance` | 600 to 100 | none |
| `FollowCamHDRecompose` | 1 to 0 | none |
| `FollowCamHDPitchGain` | 180 to 600 | none |
| `FollowCamHoldAngle` | 1 to 0 | none |
| `ManualCameraSmoothing` | 2 to 8 | none |
| `MouseSensitivityX` | 4 to 10 | none |
| `Shadow` | 3 to 0 | none |
| `AllCameras` | 1 to 0 | none |
| `FlagDisplayText` | ON to OFF | none |

`FollowCamera` is in the state hash directly, so it fails on the first tick it is compared.
`Input0_1` fails at the tick the recorded key press starts, and `bindings.digest` already catches
it before the run.

**`DetailLevel` is the one worth understanding, because it is a graphics setting that moves the
simulation.** `SetDetailLevel` ([SOURCES/GAMEMENU.CPP:632](../../SOURCES/GAMEMENU.CPP)) maps it
onto `MaxPolySea`, `Shadow`, `RainEnable` and `FlagDrawHorizon`. At 0 rain is off, and rain is a
particle system drawing from the one shared `rand()` stream everything else draws from, so the
stream offsets and the simulation follows. Deterministic at tick 235 three runs out of three.

That also explains a "none" that is not evidence of anything: `Shadow` on its own changes nothing
because `SetDetailLevel` runs at `MainLoop` entry ([SOURCES/PERSO.CPP:455](../../SOURCES/PERSO.CPP))
and overwrites `Shadow` from `DetailLevel` before the cfg value is ever read.

**The rest of the "none" column is unexercised, not proven harmless.** The recorded session walks
forward and nothing else. Every camera-shaping value in that list needs an orbit gesture, and the
mouse ones need a mouse; a recording that has one is the test that would price them. Two earlier
sweeps produced a whole column of "none" for exactly this reason and had to be thrown away: the
first ran in cube 3, and the auto camera is exterior-only, so nothing camera-related could fire.

### What the engine already knows about defaults

`T_SETTING` ([SOURCES/SETTINGS.H](../../SOURCES/SETTINGS.H)) carries a `def` per row, so
"non-default" is a comparison the engine can already make rather than a list a recording has to
maintain. Three tables declare rows:

| Table | Rows | Owns |
|---|---|---|
| `BootSettings` ([SOURCES/CONFIG_FILE.CPP](../../SOURCES/CONFIG_FILE.CPP)) | 11 | Shadow, AllCameras, FollowCamera, DetailLevel, fullscreen, VSync, dither, texture filter, FixedTimestep |
| `FollowCamSettings` ([SOURCES/FOLLOWCAM.CPP](../../SOURCES/FOLLOWCAM.CPP)) | 12 | the auto camera's recenter, HD recompose and ground-clearance block |
| `AudioSettings` ([SOURCES/AMBIANCE.CPP](../../SOURCES/AMBIANCE.CPP)) | 6 | the volumes and reverse stereo |

Both settings that broke the replay are tabulated, so a non-default capture would have named both.

**Ten settings are read by hand and are not in any table**, so they carry no declared default and a
generic sweep does not see them: `ConsoleToggleKey`, `MenuMouse`, `MouseCamera`, `MouseCameraDrag`,
`MouseSensitivityX`, `MouseSensitivityY`, `MouseInvertY`, `MouseCameraDivisor`,
`FollowCamOrbitGlide` and `ManualCameraSmoothing`. That set is almost exactly the mouse-camera
block, which is to say the settings a mouse-driven recording would be most sensitive to are the
ones a table walk would miss. Tabulating them is the cheaper half of this work and is worth doing
before the capture, not after.

Outside the tables entirely: `Language`, `LanguageCD`, `FlagKeepVoice`, `FlagDisplayText`,
`CompressSave`, the resolution pair, and the `Input*` and `Gamepad*` binding blocks. Language and
resolution are already header lines.

### The input bindings are the exception that has to be acted on

A replay injects `TabKeys` at the funnel, and `GetInput` rebuilds `Input` from that table *and* the
binding table. So the bindings are not context a replay can note and carry on with: they are half
of the function that turns the recording into actions, and the measurement above puts the cost of
getting them wrong at the first recorded key press.

`bindings.digest` detects the difference and cannot repair it. The repair is for the recording to
carry the table and the replay to install it for the duration, restoring the player's own on the
way out, which is the same borrow-and-return the snapshot reload already does for `PlayerName` and
`GamePathname`. `MAX_INPUT_SLOTS` keyboard pairs plus the gamepad pairs is a few hundred bytes
once, against a file measured in tens of kilobytes.

### Demo playback should behave like the demo reel

`Demo_RngSeed(demo_slide, timer_ref_hr, new_cube)` ([SOURCES/DEMO_SEED.CPP](../../SOURCES/DEMO_SEED.CPP))
returns the cube number while the reel is running and `TimerRefHR` otherwise, and issue #176 is the
record of why: seeding a reel from the clock made RNG-driven wanderers branch on host speed.

A replay wants that property for the same reason, and the mid-session reload already demonstrates
it. The two ends reached the same `ChangeCube` on cube 3 with `TimerRefHR` 4268125 and 4268141, a
single step apart, which is what `setup.rng_seed` exists to paper over. Under the reel's rule both
would have seeded 3 and the header line would be unnecessary.

So the better shape is that a session being recorded or replayed seeds every cube change the way
the reel does, not just the one the reload performs. That generalises to cube changes inside a
session, which `setup.rng_seed` does not reach, and it removes a header line rather than adding
one. It cannot be tested today for the reason the previous section gives: a recording cannot cross
a cube change yet.

## Portability, and playback by action

The bindings finding above is a symptom of something larger: a recording of device state is a
recording made against one player's cfg. This section prices moving the record unit to the action,
which is the `usercmd_t` shape [INPUT_DOOM3_RESEARCH.md](INPUT_DOOM3_RESEARCH.md) item F names as
the destination, and says what would have to exist first.

### What non-portability is, exactly

One thing, measured: the binding table. Rebinding forward broke a replay at the tick of the first
recorded press. `bindings.digest` detects that and cannot repair it, because a replay injects
`TabKeys` and `GetInput` rebuilds `Input` from that table *and* the bindings. Layout is not a
second problem: scancodes are positional, so a different keyboard layout reports the same codes.

### The action layer would be lossless for a real session's input

Read from the two committed real play recordings, one menu session and one 47-second play session
of 17,213 ticks over 242,000 polls:

| Key | Presses | Polls held | Binding slot |
|---|---|---|---|
| ENTER | 11 | 16,811 | 9, camera recenter |
| RIGHT | 11 | 1,773 | 3 |
| UP | 9 | 4,654 | 0 |
| LEFT | 7 | 467 | 2 |
| DOWN | 4 | 314 | 1 |
| LCTRL | 2 | 396 | 5, behaviour menu |
| W | 2 | 119 | 8, dialogue/search |
| SPACE | 1 | 75 | 7, behaviour action |

Eight distinct scancodes, and every one of them is a bound slot. Nothing in a real session, menus
and dialogue included, touched a key with no action behind it. So the residual an action stream
would have to carry alongside itself is, for that session, empty.

**And the raw-scancode sites are mostly not gameplay.** Of the 93 counted above:

| File | Sites | What it is |
|---|---|---|
| `GAMEMENU.CPP` | 38 | the menus |
| `PERSO.CPP` | 19 | the main loop |
| `CONFIG.CPP` | 14 | the key-rebinding screen |
| `INVENT.CPP` | 7 | the inventory |
| everything else | 15 | camera, dialogue, holomap, resolution switch |

The 19 in the main loop are the interesting ones, and they are almost all inside
`#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)`: the developer key block. The live gameplay site
is `if (MyKey == K_ESC OR Input & I_MENUS)` ([SOURCES/PERSO.CPP](../../SOURCES/PERSO.CPP)), which
already reads the action bit beside the scancode.

### The shape it would take, and it is not injecting `Input`

Layer C was rejected as a record unit for a reason that still holds: `Input` is ORed in from
`MainLoop`, so a modal spinning in its own `MyGetInput` loop never sees it, and the measured
453-poll modal is exactly the case a recording has to carry. Playback by action does not mean
injecting `Input`. It means:

> record the action bits; on replay re-express them into `TabKeys` through the *replaying* run's
> binding table, and inject at the same funnel as today.

Everything downstream is then unchanged, because the injection point is still layer B. The modal
sees a key table. The 93 raw-scancode sites see a key table. What changes is that the file says
"the player asked to move forward" rather than "scancode 82 was down", so a player bound to WASD
replays a recording made on the arrow keys.

### What does not exist yet

**The reverse lookup.** `InitInput` folds `DefKeys` and `GamepadKeys` into one flat
(key, mask) table scanned to build `Input`. Nothing goes the other way; a grep for an
action-to-key direction returns nothing. `KeysFromBinding` and `BindingFromKey` are named as a
take in [INPUT_DOOM3_RESEARCH.md](INPUT_DOOM3_RESEARCH.md) item C, for the controls screen and for
profiles, so this is a shared prerequisite rather than recorder-specific work.

Three things the reverse direction has to answer that the forward one does not:

- **A slot has up to four keys** (`DefKeys` Key1/Key2 plus the gamepad pair) and re-expression
  picks one. That is where bit-exact device replay is given up: the recording stops being a record
  of what the device reported and becomes a record of what the player asked for. Doom 3 made the
  same trade deliberately, and it is the reason a `.cdemo` is portable where a journal is not.
- **`MyKey` is a raw scancode that menus compare**, so it has to be synthesised alongside the
  table rather than falling out of it.
- **Slots 32 to 35, the spells, never reach `Input`** and are read through
  `CheckKey(DefKeys[..])` in `PERSO.CPP`. An action unit is therefore 36 slots wide, not `Input`'s
  30 bits.

**A residual channel for unbound keys.** Empty for the play session measured above, and not empty
in general: the console toggle, the developer block, and the key-rebinding screen itself, which is
the sharp case, since a recording of a player rebinding keys cannot be expressed in bindings.

### The reverse lookup, measured against the tables it would run on

The forward direction is `held(slot)` = `CheckKey` over up to four keys, and the reverse has to
produce a key table a second `held()` reads back identically. Three things decide whether that is
sound, and all three are readable from the tables rather than arguable.

**The predicate is already uniform across all 36 slots.** `SpellKeyDown`
([SOURCES/PERSO.CPP](../../SOURCES/PERSO.CPP)) is the same OR of `DefKeys` Key1/Key2 and
`GamepadKeys` Key1/Key2 that the combined table performs for slots 0 to 31. The only difference is
that the spells have no `Input` bit to live in: `INPUT_TABLE_SLOTS` is 32, `Input` uses all 32 bits
(`I_UP` through `I_WEAPON_7`), and `I_PINGOUIN` through `I_FOUDRE` are a *second* mask that starts
again at bit 0. So an action unit is 36 bits and one rule, not 32 bits plus a special case.

**The shipped table is injective, so the naive reverse is exact on it.** Read off the cfg the
engine writes:

| | |
|---|---|
| slots with at least one key | 36 of 36 |
| distinct keys used | 49 |
| keys bound to more than one slot | **none** |
| slots with no key at all | **none** |

So "press the first bound key of every held slot" round-trips exactly through `held()` for the
layout every player starts from.

**It is not injective by construction, and both failures are visible before a replay starts.** A
cfg may put one key on two slots, in which case pressing it for the held slot also raises the other;
and a cfg may leave a slot with all four keys zero, in which case a held slot cannot be expressed at
all. Both are a scan of the table, which is why the reverse belongs in the bindings module as a
checked function that can refuse, rather than as a best-effort helper that quietly produces a
different frame.

### What a residual channel has to carry, and why it is already portable

Across every raw scancode comparison in `SOURCES`, 47 distinct keys are compared literally and 24
of them are in no slot of the default table. `K_ESC` alone accounts for 24 of the 93 sites, and it
is live: `if (MyKey == K_ESC OR Input & I_MENUS)` is the cinema and menu cancel.

Most of the rest are the developer block. Of `PERSO.CPP`'s 19 sites, 15 sit behind `DEBUG_TOOLS`,
`TEST_TOOLS` or `ENABLE_POLY_RECORDING`; the four a release build reaches are `K_F9` twice (the
screenshot), `K_T` under `DemoSlide`, and the `K_ESC` above.

**And here is the part that makes the hybrid worth having: a key compared literally in the source
cannot be rebound, so it means the same thing on every machine and under every cfg.** There is
nothing to translate. A recording of 36 action bits plus a residual set of raw scancodes is
therefore fully portable *without carrying a dictionary at all*, where today's file is portable
only because it carries one. That also means the move does not have to be all or nothing: the
bound half becomes actions, the hardcoded half stays raw, and each is portable for its own reason.

A recording made with `K_ESC` bound to a slot carries it twice, once as the action and once as the
residual, and both re-express to the same key. That is consistent rather than a conflict.

### Where it belongs, and what it is worth on its own

`INPUT_BINDINGS.{H,CPP}` is where the tables already live, extracted by
[INPUT_PLAN.md](INPUT_PLAN.md) increment 0 precisely because it is pure and host-testable, and it
is the module that owns `InitInput`'s forward fold. `KeysFromBinding` and `BindingFromKey` are
already a take in [INPUT_DOOM3_RESEARCH.md](INPUT_DOOM3_RESEARCH.md) item C, wanted by the controls
screen (which indexes `DefKeys` positionally today) and by a profile system, neither of which is
this recorder. So the reverse lookup is shared work with an existing customer, and a recording
format is its second reader rather than its reason.

Built, in [SOURCES/INPUT_BINDINGS.H](../../SOURCES/INPUT_BINDINGS.H), with
[tests/input_bindings/test_input_reverse.cpp](../../tests/input_bindings/test_input_reverse.cpp)
beside it:

```c
S32 Bindings_KeysForSlot(S32 slot, U32 *out, S32 max); /* keyboard pair first, then pad */
S32 Bindings_SlotForKey(U32 key);                      /* -1 when the key carries no slot */
S32 Bindings_CheckReversible(S32 *badSlot, U32 *badKey); /* the two failures above */
```

The test's central claim is that the two directions agree rather than that either is
self-consistent: for every slot the retail layout binds, a key the reverse hands back is one the
forward fold raises that slot's bit for and no other's, driven through the real `GetInput`. It also
pins the deliberate differences. The reverse covers all 36 slots where the fold stops at 32, since
the spells are bound the same way and a lookup that inherited the ceiling would report the penguin
as unbound. It reads the source tables rather than the folded one, so a rebind is visible to it
before `InitInput` runs and not to the fold, which is a real difference a caller can trip on. And
both reversibility failures are refused with the offender named, with the empty slot outranking a
sharing pair further down the table, because "this action has no key" is the answer someone is
looking for and burying it under a later fault hides it.

Nothing consumes the reverse yet. It is the piece an action-shaped recording would rest on, and
the controls screen and a profile system are the readers that wanted it first.

### The sequencing, and what shipped

Item D above already states the property that settles the order: **the frame intent is a pure
function of the polled state plus the binding table**. So a recording that carries the binding
table can be converted to an action stream offline, later, without being re-recorded, on the day
the reverse lookup exists. One that does not carry them cannot.

That makes carrying the table the first move rather than a competing one, and it is what the
format now does. `bindings.keyboard` and `bindings.gamepad` hold the two tables as
`k1:k2` pairs, one line each, and a replay installs them before its first replayed poll and puts
the player's own back when it stops. Format version 7.

Measured, replaying one recording under a cfg that moved the bindings:

| Replaying run's bindings | Result |
|---|---|
| the recording's own | clean, three runs out of three |
| forward moved from UP to W | **clean, three runs out of three** (was: failed at tick 33) |
| forward, back, left and right all moved | clean |

And the borrow returns. Reading `bindings.digest` off the live run at three moments in the same
rebound session: `be9bc958` before the replay, `f5e20ec4` (the recording's) during it, `be9bc958`
again after it ends.

**What it does not buy.** The file still says "scancode 82 was down" rather than "the player asked
to move forward", so it is portable because it carries its own dictionary, not because it stopped
needing one. A recording is still tied to this engine's scancode meanings and to the 36-slot
binding layout. That is the part moving the record unit would fix, and the part this defers rather
than solves.

## What this buys beyond reproducing input

The uses are what justify the format work, and three of them do not involve replaying a player.

**Fixtures.** 62 control-harness fixtures drive a real engine, 16 of them scripting the run
with `--exec-at`, and [tests/automation/README.md](../../tests/automation/README.md) is explicit
that no workflow runs any of them. A recording is the same fixture recorded instead of
written, and unlike a script it carries its own assertion, so a fixture stops being a script plus
a separately-maintained expectation. The combo fixtures [INPUT_PLAN.md](INPUT_PLAN.md) increment 1
specifies (running jump, behaviour-cancel, hold against re-press) are frame-exact by nature and
are exactly the shape that is easier to record than to write.

**New attract demos.** The reel is authored Track and Life bytecode inside each scene, so today
adding one means writing `TM_*` and `LM_*` programs into scene data, which is what issue #181's
disassembler and assembler exist to make possible. A recording is a second route that needs none
of that: play the scene, ship the file. It also sidesteps the reason the reel is fragile, since
`Demo_RngSeed` had to be introduced to stop RNG-driven wanderers pushing Twinsen off his authored
track. A recorded demo does not follow a track, so it cannot be pushed off one, though it does
need its seed pinned for the same reason everything else does.

**Scenarios and bug reports.** A self-describing file a player can attach. This is the use that
depends most on the hash, because a report that replays differently on the maintainer's machine
should say so at a named tick rather than look like it reproduced.

**Making socket sessions reproducible.** [CONTROL.md](../CONTROL.md) states that a `--listen`
session is "not deterministic, by construction", because commands arrive at whatever tick the
driver sent them, and steers anything reproducible back to `--exec-at`. Recording the poll stream
removes that trade: an exploratory session driven live becomes a file that replays, so finding a
repro interactively and turning it into a fixture stops being two separate jobs. This is the use
with the best ratio of value to added scope, because the socket and the recorder would share
nothing except the engine they both already attach to.

**Locating cross-platform divergence.** Issue #530 established that the RNG stream differs
between glibc and the Windows CRT, and [SPEEDRUN_MECHANICS.md](../SPEEDRUN_MECHANICS.md) already
names that as the limit no compatibility mode removes: a recording is not guaranteed to replay on
a different operating system however faithful the game logic is. A hash timeline does not fix
that, but it turns a detected difference into a located one, naming the first tick where two
platforms stop agreeing.

## What not to take from Doom 3

**The `usercmd_t` refactor as a prerequisite.** id needed one struct because keyboard, mouse and
joystick arrived separately and gameplay read all three. Here `GetJoys` already writes into the
same table the keyboard uses, and `GetInput` already folds pad and keyboard through one combined
binding table. The waist exists.

**Absolute view angles in the sample.** Rejected for the same reason
[INPUT_DOOM3_RESEARCH.md](INPUT_DOOM3_RESEARCH.md) rejects them: the hero turns and the camera
follows, and the camera's rules are established by measurement in [CAMERA.md](../CAMERA.md).

**Two record systems at two layers.** Doom 3 has both the journal and command demos because the
journal was already there from Quake 3. Here the journal layer is measurably unusable, so there
is one layer to build.

**A per-tic index into a ring buffer.** `MAX_BUFFERED_USERCMD` and `TicCmd(lastGameTic)` exist so
a consumer that falls behind reads the commands it missed. Nothing here falls behind: the
consumer is the same loop that produced the sample, in the same iteration.

## Prior art inside this tree

Polyrec ([POLYREC.md](../POLYREC.md)) is a record and replay system at the draw-call layer,
shipped and in use. It is the closer model on three counts: it records
at the narrowest complete waist it could find (three primitives, chosen so the scene graph and
projection need not be captured); its file carries a `REFF` section holding the framebuffer the
game itself produced, which is the same "the recording is both the fixture and the assertion"
property as the consistency hash; and its container is a magic plus a version plus named
sections, which is a format shape already familiar in this repo.

The differences are just as instructive. Polyrec captures one frame, so it never had to answer
the index or the clock question, and it compares one artifact at the end rather than a timeline.
A session recorder is the same idea extended along time, and the two questions time adds are the
two this doc spends its measurements on.

## Open questions

1. **What is the sample change rate under a real player?** The prototype answers this for
   harness-driven play, where key presses are sparse and a poll costs about a byte. A human on a
   keyboard and a mouse is a different signal, and the mouse is the part that will not compress.
   Recording an ordinary play session and reading the same counters answers it.

2. **Does a recording made under real play conditions replay?** Answered: not on a clock read
   from the host, and no amount of reconstruction fixes it, because the engine needs a constant
   dt rather than a reproduced one. A session recorded on a fixed timestep replays exactly, four
   times over and four at once under load. So the question for a player's session is not whether
   it can be captured but whether pinning the timestep while recording is acceptable to play on,
   which is a design call rather than an open measurement.

3. **What belongs in the hash?** The `--dump-state` field set was used here because it exists and
   is already the baseline corpus's notion of state. It excludes extras, projectiles and sound
   state. The prototype showed the cost of getting it wrong is silence rather than noise: an
   injected input that changed nothing in the field set was invisible at any digest width. Running
   it against the existing corpus decides it; choosing does not.

4. **How does a recording interact with `--fixed-timestep`?** The throttle makes one rendered
   frame drive zero or many sim steps, and `Timer_ForceStepIfPending` promotes a would-skip frame
   on an action edge. A recording indexed in polls reproduces the frames; whether it reproduces
   the promotions depends on the edges resolving identically, which they should, since they are a
   function of `Input`, which is a function of what was recorded. Worth confirming rather than
   assuming.

5. **Should a recording be able to start from a mid-session state rather than a save?** Attaching
   a `--dump-state` snapshot as the setup would let a recording start anywhere. It would also mean
   the format carries a second, larger notion of state that has to stay in step with the engine.
   Probably not worth it, but it is the question a scenario library would ask first.

## Reproduce

The measurements above came from a temporary instrumented build: a counter at the tail of
`UpdateKeyboardState()` in [LIB386/SYSTEM/KEYBOARD.CPP](../../LIB386/SYSTEM/KEYBOARD.CPP), a
counter in `MyGetInput()` in [SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP), and an FNV-1a digest
plus a `clock_gettime` pair in `Control_TickHook` in
[SOURCES/CONTROL.CPP](../../SOURCES/CONTROL.CPP). None of it is committed; each site is named so
a guarded `fprintf` can be re-added.

```bash
cmake -S . -B build-instr -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSOUND_BACKEND=null
cmake --build build-instr -j"$(nproc)"

# Polls per tick, gameplay: 1 on every row.
./build-instr/SOURCES/lba2cc --headless --no-autosave \
    --load "$LBA2_TEST_SAVE" --fixed-dt 16 --tick 200 --exit

# Polls per tick, a modal: 453 at tick 877, and identical across repeats.
./build-instr/SOURCES/lba2cc --headless --no-autosave \
    --demo --exec "cube 193" --fixed-dt 16 --tick 900 --exit

# Hash timeline reproducibility, and the perturbation that locates a change.
./build-instr/SOURCES/lba2cc --headless --no-autosave \
    --load "$LBA2_TEST_SAVE" --fixed-dt 16 --tick 600 --exit
./build-instr/SOURCES/lba2cc --headless --no-autosave \
    --load "$LBA2_TEST_SAVE" --fixed-dt 16 --tick 600 \
    --exec-at 300 "input up 60" --exit
```

The prototype's own results came from a `-DLBA2_CONTROL_SERVER=ON` build carrying an
uncommitted `SOURCES/RECORD.{H,CPP}` plus hooks at the three sites named above:

```bash
# Record a session driven through the binding layer, then replay it with nothing driving it.
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 16 --tick 400 \
    --exec "rec start s.rec" --exec-at 50 "key up 120 2" --exec-at 399 "rec stop" --exit
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 16 --tick 400 \
    --exec "rec play s.rec" --exec-at 399 "rec info" --exit

# The regression, and the one to run first: record a session, replay it, expect no
# consistency failure. Two runs, and the second one's summary is the whole result.
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 16 --tick 300 \
    --record f.rec --exec-at 30 "key up 60 2" --exit
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 16 --tick 400 --replay f.rec --exit
# expect: first hash mismatch -1, clock drift max 0 ms

# The same from boot, with no --load at all. Headless it needs skipmodals: cube 0 reaches
# the opening house dialogue around tick 200, and that waits on a keypress no headless run
# sends. DemoSlide would also clear it, but --demo reseeds ChangeCube's srand and so changes
# what is being recorded. Run with --verbose if it hangs anyway: the modal marker names the
# blocker, and no [control] line at all means it never reached the first tick.
lba2cc --headless --no-autosave --fixed-dt 16 --tick 300 --exec "skipmodals 1" \
    --record b.rec --exec-at 30 "key up 60 2" --exit
lba2cc --headless --no-autosave --fixed-dt 16 --tick 300 --replay b.rec --exit

# A recording made under a boot-time --load is not self-contained: setup.reloaded is 0,
# because that flag means the recording reloaded its own snapshot mid-session. Replay it
# with the same --load or it starts from cube 0 and mismatches at tick 0.

# The same for a mid-session recording, which is the path that reloads its own snapshot.
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 16 --tick 400 \
    --exec "rec start m.rec" --exec-at 50 "key up 120 2" --exec-at 399 "rec stop" --exit
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 16 --tick 500 \
    --exec "rec play m.rec" --exit

# Give the replay more ticks than the recording holds. Ending on --tick before the stream
# runs out means the summary never prints, and a run that reported nothing reads as a run
# that passed.

# Which settings break a replay. Needs an exterior scene with the auto camera on, or the
# camera code under test never runs: cube 3 is an interior and a whole sweep there comes
# back clean for that reason alone. Baseline first, then one cfg key per run.
lba2cc --headless --no-autosave --user-dir U --load "$DOWNTOWN" --fixed-dt 16 --tick 300 \
    --record dt.rec --exec-at 30 "key up 150 2" --exit
sed -i 's/^DetailLevel:.*/DetailLevel: 0/' U2/lba2.cfg
lba2cc --headless --no-autosave --user-dir U2 --load "$DOWNTOWN" --fixed-dt 16 --tick 400 \
    --replay dt.rec --exit

# The cube-change wedge: the first completes, the second hangs.
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 16 --tick 320 --exec "cube 49" --exit
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 16 --tick 320 --exec "cube 49" \
    --record w.rec --exit

# The modal: 453 polls inside one tick, recorded and replayed.
lba2cc --headless --no-autosave --demo --exec "cube 193" --fixed-dt 16 --tick 900 \
    --exec-at 2 "rec start modal.rec" --exec-at 899 "rec stop" --exit

# Mode mismatch: named before the run, then caught at poll 1 and tick 4.
lba2cc --headless --no-autosave --load "$SAVE" --fixed-dt 20 --tick 400 \
    --exec "rec play s.rec" --exec-at 399 "rec info" --exit

# Windowed, driven over the socket. Without --listen implying it, moving focus to the
# driving terminal stops the clock.
lba2cc --no-autosave --load "$SAVE" --listen 4444
scripts/dev/lba2ctl.py 4444
```

Figures read straight from the tree, no build needed:

```bash
grep -n 'TABKEYS_NUM_KEYS' LIB386/H/SYSTEM/KEYBOARD.H          # 256 + (16 * 8)
grep -n 'MAX_VARS_GAME\|MAX_VARS_CUBE' SOURCES/COMMON.H        # 256, 80
grep -rnE '(MyKey|Key) *== *K_' --include=*.CPP SOURCES | grep -v INPUT.CPP | wc -l   # 93
grep -rn 'CheckKey(' --include=*.CPP SOURCES | wc -l           # 21
grep -rn 'MyGetInput()' --include=*.CPP SOURCES | wc -l        # 81
ls tests/automation/test_*.sh | wc -l                          # the fixture count
grep -n 'JoystickGetRightStick' SOURCES/EXTFUNC.CPP             # the analog camera read
grep -n 'FlagMouseCamera = ' SOURCES/GLOBAL.CPP                 # TRUE by default
```
