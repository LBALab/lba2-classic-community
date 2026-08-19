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

Built and measured: a session driven by real key presses replayed bit-for-bit from the file, a
modal carrying 453 polls inside one tick replayed in order, an existing fixture's trajectory
replayed four times faster than the fixture runs, and a perturbed replay reported the tick it
stopped matching. 82 added lines across 10 existing files, plus one module.

## The four layers a recording could sit at

| Layer | Unit | Complete? | Verdict |
|---|---|---|---|
| A. SDL events | `SDL_Event` | no | reject, measured below |
| B. Polled device state | `TabKeys[384]` + `Key` | digital yes, analog no | **take**, plus 20 bytes |
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

[INPUT_PLAN.md](INPUT_PLAN.md) item F names one small struct saying what the player asked for
this tick, built at one place, as the destination the input increments head toward. A recording
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

Pinning the game clock to one sample per input poll at record time makes variable-rate replay
exact. It also arms the harness virtual clock on a normal player session, where it stops input
responding at the boot banner, which is how far a recorder can be from passive while still
looking like it works.

The measurement that settles it is one A/B: run the same scripted session with and without the
recorder and diff the state. Pinned, hero animation state differs. With the capture inside
`ManageTime`, every field is identical.

That test belongs beside the format, not in a commit message. A recorder that perturbs the run it
observes produces recordings of a game nobody played.

### What the prototype does not do

Replay a real play session past its first scene load, as above.

Run-length encode the clock stream, which is the single change that would most reduce a
recording, and give that stream an index of its own so a difference in `ManageTime` call count is
reported rather than survived.

Verify anything in the menus. The state hash is written from `Control_TickHook`, which only runs
in `MainLoop`, so a 25-second menu recording carries 19,736 polls and zero ticks. Menu navigation
records and replays; nothing checks it. The existing UI capture goldens are the oracle that
would.

It also carries the mouse at the keyboard poll index rather than at `ManageMouse`'s own, so the
mouse delta a sample holds is the one drained on the previous frame. Consistent between record
and replay, and wrong if anything ever reads the two at different rates. A real format would give
the mouse its own index or move the drain.

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

2. **Does a recording made under real play conditions replay?** Half answered. A real windowed
   session with audio replays its entire menu portion, 76,587 polls, with no hash mismatch, and
   then fails at tick 2 of the first scene load. What is not yet separated is whether that is the
   environment or the format, because a replay with the audio thread alive could not be run on
   the machine that recorded it. Running one is what closes this.

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
