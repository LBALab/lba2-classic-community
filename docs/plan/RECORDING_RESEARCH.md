# Recording: research

Where a Doom 3 style recording system should attach in this engine, what it would cost, and
what it buys beyond reproducing input. Written as findings and one recommendation; it commits
nothing and schedules nothing.

Follows [INPUT_DOOM3_RESEARCH.md](INPUT_DOOM3_RESEARCH.md) item E, which named command demos
with a consistency hash as the replay design worth taking, and
[INPUT_REPLAY_RESEARCH.md](INPUT_REPLAY_RESEARCH.md), which established that no recorded input
ships with the game and left the seam, the record unit and the file format open. This doc
answers those three.

Every figure below was measured against the working tree with a temporary instrumented build.
The commands are at the end. Re-run them rather than trusting a number that has aged.

## The short answer

Record the polled device state at the tail of `UpdateKeyboardState()`, indexed by input poll
rather than by tick, with the clock delta stored beside each sample and an FNV-1a hash of
simulation state stored beside each tick.

That is one function, and the hook that replays it already exists there for another reason. It
covers the whole digital funnel and three analog values sit outside it, priced below.

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
above is 408 bytes per poll, and gameplay was measured at exactly one poll per rendered frame, so
a raw stream is about 24 KB per second at 60 fps. A change-only encoding is the obvious fix for the table, which is unchanged on the
large majority of polls in any real session. It does nothing for the mouse deltas, which are new
on every poll the mouse is moving, so the analog 20 bytes are close to the floor rather than
close to free. No compression figure is quoted here because the change rate in real play has not
been measured. It is listed as an open question.

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

1. **What is the sample change rate in real play?** It decides the encoding and the file size,
   and it has not been measured. A recorded session of ordinary play, counting polls where
   `TabKeys` differs from the previous poll, answers it directly.

2. **Does a recording made under real play conditions replay?** Everything above says the clock
   is handled and the audio thread is not. The experiment is cheap once record and replay exist:
   record windowed with audio, replay headless with the null backend, and read the tick the hash
   first disagrees at. That number is the answer, and it is more useful than any argument about
   it.

3. **What belongs in the hash?** The `--dump-state` field set was used here because it exists and
   is already the baseline corpus's notion of state. It excludes extras, projectiles and sound
   state. Whether it catches the regressions worth catching or trips on noise is decidable by
   running it against the existing corpus rather than by choosing.

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
