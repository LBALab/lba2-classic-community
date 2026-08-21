# Session recording

Record a play session and replay it into the same simulation, with the engine reporting the first
tick where the two stop matching.

A recording is the player's input, the console commands the session was driven with, a snapshot of
where it started, and a per-tick digest of simulation state. That last part is what separates this
from a macro: a replay does not merely re-press the keys, it checks that the game reached the same
place, and says exactly where it did not.

The design work behind it, and what was measured rather than assumed, is in
[plan/RECORDING_RESEARCH.md](plan/RECORDING_RESEARCH.md).

## Recording and replaying

```bash
# Record a session you play yourself. Quit through the menu, or type `exit` at the console.
lba2cc --fixed-dt 16 --load "/path/to/save.lba" --record session.rec

# Replay it. Same --load, because the recording does not reload its own snapshot (see below).
lba2cc --fixed-dt 16 --load "/path/to/save.lba" --replay session.rec --tick 4000 --exit
```

A clean replay ends with `first hash mismatch -1`. Anything else names the tick:

```
[rec] consistency failure at tick 296: recorded 61b1b567…, replayed aa417b83…
[rec] replay ended at poll 10305: 397 ticks checked, first hash mismatch 296, clock drift max 0 ms
```

Mid-session, from the console (F12), which records from a point you choose rather than from boot:

| Command | What it does |
|---|---|
| `rec start [name] [verbose]` | Write a snapshot, reload it, and start recording from the post-load state. `verbose` adds the telemetry below |
| `rec stop` | Stop and flush |
| `rec play [name]` | Replay a recording into the running engine, then put your game back |
| `rec info [name]` | Report the current session, or compare a file's mode lines against this run |

The name is optional at both ends, and without one nothing has to be typed or kept track of:

```
> rec start
rec: recording to /home/you/.local/share/Twinsen/LBA2/recordings/session-20260820-150408.rec
> rec stop
rec: stopped
> rec play
rec: replaying /home/you/.local/share/Twinsen/LBA2/recordings/session-20260820-150408.rec
```

Playback borrows the session and gives it back. `rec play` saves where you are before it loads
the recording's world over yours, and returns you there when the replay ends or you stop it, so
watching a recording does not cost you your game. `--replay` does not, and does not need to: that
run exists to replay and exit, and has no session to protect.

`rec start` names the session after the time of day. `rec play` takes the recording this run
recorded, and in a session that has recorded nothing -- the next launch, say -- the most recently
written one in the folder.

Neither command needs a flag either. A mid-session `rec start` pins the simulation step itself, on
the reload it already performs, so the recipe below is for the from-boot flags and not for this.

## Where recordings live

`<userDir>/recordings/`, beside `save/`. The boot banner names it, under `Saves:`, so a pasted log
says where a run's recordings went without anyone having to know the layout:

```
Saves:  /home/you/.local/share/Twinsen/LBA2/save/
Recs:   /home/you/.local/share/Twinsen/LBA2/recordings/
```

An argument with no directory in it is a name in there, which is
why the commands above take a name and not a path: a session recorded as `session.rec` replays as
`session.rec`, from whatever directory the run is started in.

Anything with a separator in it is a path and is used as given, so `./session.rec` is still the one
here and `/tmp/x.rec` is still `/tmp/x.rec`. The run prints the file it resolved to, which is the
one to go and find:

```
[rec] recording to /home/you/.local/share/Twinsen/LBA2/recordings/session.rec
```

Reading has one more rule, and it fires only where the folder has nothing: a bare name the
recordings folder does not have, but the working directory does, is taken from the working
directory. It cannot pick the wrong file, because a folder that has the name wins.

`--user-dir` and `--profile` move the folder with the rest of the profile, so recordings made under
one profile are the ones that profile replays.

## The pinned step is still required, and not for the reason it looks like

A session recorded on the host-sampled clock does not yet replay reliably, so record and replay
with `--fixed-dt 16`. The reason is not the clock.

A loose recording reproduces `TimerRefHR` at **0 ms drift at every tick, in every run measured**,
so the host clock is not what a replay fails to reproduce. What `--fixed-dt` does is make every
frame delta exactly 16 ms, which makes every value derived from frame timing trivially equal at
both ends. That is a masking mechanism rather than a determinism one, and what it masks is
simulation state no save, digest or recording carries. Each such value found is one less thing the
pinned step has to hide, and the day the list is empty the step can go.

Two are closed, both invisible under a pinned step by construction:

- `LastSimRefHR`, the [#358](MOVEMENT_FRAMERATE.md) sub-step carry, decides where a frame's
  sub-step boundaries fall. A replay that begins on its own boot's value rather than the
  recording's diverges at tick 1 by a millisecond, and an actor is on a different animation two
  hundred ticks later. At a constant 16 ms `Timer_PlanSimSteps` always returns one step and the
  carry cannot matter, which is why a pinned session cannot see it either way. It lives in
  `TIMER.H` beside `TimerRefHR`, the recording declares it as `clock.sim_carry`, and a replay
  restores it beside the baseline.
- `VoiceHeardBySim` answers from the sample's own length on the game clock for any run that has to
  reproduce, not only a pinned one. Gated on the step, a loose session asks the mixer instead, whose
  clock no replay reproduces. See "Audio used to move the answer" below.

A fourth is closed and is **not** the same shape as the others, which is what makes it worth
reading. Everything above is per-frame timing state. This is a **seed**. `ChangeCube`
([SOURCES/OBJECT.CPP](../SOURCES/OBJECT.CPP)) seeds the one shared `rand()` stream from
`TimerRefHR`, and on the `--load` path it does so about two hundred lines before `LoadGame`
installs the savegame's clock, so the seed is **how many milliseconds the process took to
boot**. Two runs do not boot in the same number of them. Measured on one byte-identical
recording replayed fourteen times on an idle machine: seed 40 eleven times and seed 41 three
times, and the one-millisecond difference decided the verdict every time, clean at 40 and
diverging at tick 225 at 41. `--fixed-dt` hides it completely, because `Timer_EnableFixedDt`
zeroes `TimerRefHR` and every pinned run therefore seeds with 0.

A replay cannot ask the file for that seed at the moment it needs it: the recorder starts on
the first input poll, which is after the boot scene load, and by then the stream has been
seeded and drawn from. So the recording carries it as `clock.rng_seed`, `--replay` reads that
one field when the flag is parsed, and `Record_SeedHook` hands it over when the load asks.
A recording without the field replays exactly as it did before. Measured over twelve fresh
recordings replayed twice each: **reproduced 4 of 12 before and 10 of 12 after**, and the two
replays of one file disagreed on **5 of 12 before and 0 of 12 after** (Fisher two-sided
p = 0.036 and p = 0.037).

A third was the same shape and is closed with them: `InitAnim` stamps the hero's animation anchors
during boot, from wall-clock time, and `LoadGame` installs the save's clock afterwards. Every other
actor is stamped after that line and comes out on the restored reading, so the hero alone carried
whatever the wall clock said between process start and scene init. `LoadGame` now puts him on the
restored clock with his frame interval intact.

Reproducing the clock more faithfully is finished as a direction. Storing one sample per
`ManageTime` call was built and measured and makes replays *worse*. What is left is
finding the state, and the digest names it: see `numeric.digest` and the `sim.carry`,
`obj.LastTimer`, `obj.NextTimer` and `rng.draws` fields.

### A wait loop that never polls needs a clock that moves

`Record_ClockHook` holds `ManageTime` at the last input poll, so a loop that ends on the clock and
does not poll reads one value for as long as it runs. `FadeToPalAndSamples`
([SOURCES/AMBIANCE.CPP](../SOURCES/AMBIANCE.CPP)) is exactly that shape: it ends when the reading
has moved `FADE_DELAY`, and it never polls. Held, it does not end at all, and the run spins at a
full core presenting a frame an iteration until something kills it. That is the first scene change
of any recording on the host clock, and the first fade after the menus when recording from boot.

The wait loops already declare themselves. `Timer_FixedDtPump()` marks the ten places that end on
the clock without polling, and under `--fixed-dt` it is what mints the step that ends them, which
is why the pinned clock never meets it. A recording on the host clock mints one there too:
a fixed 16 ms per iteration, with the real time that step stands for slept out, so the fade lasts
what it lasts and the loop ends.

Fixed rather than measured, and that is the part worth keeping. Advancing the held reading by real
elapsed time also ends the loop, and the fade itself is transparent either way, because these loops
sit inside `SaveTimer`/`RestoreTimer` and `TimerRefHR` is rewound whatever the loop did to it. What
is not transparent is the last reading the loop leaves behind. `ManageTime` banks the next delta
against it, so a fade that ran a few milliseconds longer on one end than on the other puts those
milliseconds into the game clock after the rewind, which is enough to plan one more simulation
sub-step. Measured with a measured step: `sim.carry` 16 ms apart and `obj[2].X` 32 units apart at
the first tick after the scene change, in seven replays out of ten. With a fixed step both ends run
the same number of iterations and leave the same reading, so there is no difference to bank.

All ten sites are bracketed, `MUSIC.CPP`'s two volume fades and `RES_SWITCH.CPP`'s dialog included,
so the rewind is not what separates them. What survives a rewind is `LastTime`, and nothing restores
that: a rewind that puts back one of a pair of coupled variables is not a rewind.

**This makes such a session recordable, and does not make it reproduce.** See the rate below: a
loose recording reproduces some of the time whether or not it crosses a fade, and the wedge was
hiding the scene-change half of that question rather than answering it.

Who pins it depends on where the recording starts, and the split is not a convenience:

- **From boot, with `--record`,** the run has no load to hide a clock reset behind, so the step has
  to be on the command line: `--fixed-dt 16`.
- **Mid-session, with `rec start`,** the recorder pins it, and gives it back on `rec stop`
  or when a replay ends. It can, because it already writes a
  snapshot and reloads it, and `Timer_EnableFixedDt` zeroes `TimerRefHR` -- which is safe only where
  a load follows to put the clock back. A player whose session is already in progress cannot be
  told to relaunch, so this is the path that makes recording something they can reach.

A replay pins whatever step the recording's header names, so a session played at some other step
replays at that one rather than at the default.

The step is given back where it was taken. A pinned step advances game time by dt per rendered
frame rather than by wall clock, and the clock is held to real time only while a recording is
running, so a session that kept the step afterwards ran at whatever rate it rendered at --
measured headless, 1.00x real time before a recording and 3.12x after one. `rec stop` and the end
of a replay hand it back, and a run given `--fixed-dt` keeps its own: that step belongs to the
whole run and to whoever asked for it.

## Holding a recorded session to real time

A pinned clock hands out its step from three places, not one: the tick advance, an extra present
inside a frame, and a non-presenting pump. `Timer_SetFixedDtPaced` makes each of them cost a step
of wall time, because the pacing lives at the single point in `TIMER.CPP` that all three go
through.

Not per tick, which is the only unit a caller outside the timer can see and the one unit that
misses every modal. A fade, a menu, a dialogue box and an inventory screen each run their own loop
that presents and pumps without ever reaching the main-loop tick, so a pacer sitting on the tick
leaves all of them free to draw as much game time as they like at no cost in real time.
`FadeToBlack` (`SOURCES/AMBIANCE.CPP`) waits out `FADE_DELAY`, 200 ms of game time; paced at the
tick it spends 26 ms of wall clock doing so, eight times real speed. That is what the too-fast
transitions are, along with the save menu that scrolls several entries to a keypress, the loading
that fast-forwards and the dialogue that skips itself before its voice line finishes. Paced at the
mint site the same fade spends 208 ms of wall for 208 ms of clock.

None of this moves the step *sequence*. Each of the three sites mints the same number of steps
whether the clock is held to real time or not, which is what makes pacing safe for a format that
compares per-tick hashes: it spends wall time and changes no value the simulation reads.

Off unless a recording asks for it. A batch run under `--fixed-dt` wants the opposite, and
outrunning real time is most of why the harness has the flag.

Two things it does not cover. A **replay** is not paced, which is right for verifying a recording
and wrong for a player watching one back with `rec play`, where it races. And `FadeToBlack` pumps
*and* presents on each iteration, so it mints 32 ms a time rather than 16 and renders half the
frames it should; pacing gives it the right duration but not the frames, and correcting the double
step moves the clock sequence, so it wants its own change.

A recording whose step the recorder pinned carries `setup.reload_clock=0`, where one made
under the flag carries the real reading. That is not a defect and not worth reconciling:
`Timer_EnableFixedDt` zeroes `TimerRefHR`, so on that path the reload genuinely is
performed at clock 0, on both ends, every time. The post-load `clock.timer_ref_hr` is
identical either way, because the savegame is what restores it.

**Both ends have to arm it the same way, and matching values is not enough.** Arming at boot and
arming at the reload are different points in the run, and `Timer_EnableFixedDt` reseeds the clock
where it is called, so a recording made one way and replayed the other diverges even at the same
step. Measured on the same save and the same 16 ms step:

| Recorded | Replayed | Result |
|---|---|---|
| `--fixed-dt 16` | `--fixed-dt 16` | clean, `first hash mismatch -1` |
| no flag, `rec start` pins it | no flag, `rec play` pins it | clean, `first hash mismatch -1` |
| `--fixed-dt 16` | no flag | diverges at tick 1, 3152 ms of clock drift |
| no flag, `rec start` pins it | `--fixed-dt 16` | diverges at tick 1, 3152 ms of clock drift |

So a session recorded with the flag is replayed with the flag, and one recorded without it is
replayed without it. The header records the step but not where it was armed, so the mode comparison
cannot yet report the two bottom rows: after arming, both ends say `mode.fixed_dt=16` and the
comparison is looking at the same number on both sides. Recording where the step was armed, so a
replay can name that difference the way it names the others, is the open piece.

The header records the mode either way, and `rec info` compares a recording's mode lines against the
live run, so a replay under a step the recording was not made at is reported rather than silently
wrong.

## What a recording contains

| Part | Why |
|---|---|
| Polled device state, one sample per input poll | Indexed by poll rather than by tick, because a modal can spin thousands of polls inside a single tick |
| The mouse, the right stick and the pad's first-pressed scancode | None of the three has a scancode, so none rides the key table the rest of the input rides. Twenty bytes on the polls that carry any of them, nothing on the polls that do not |
| The console commands the session was driven with | Otherwise a recording cannot stand in for a harness-driven fixture |
| The keyboard and gamepad binding tables | A replay borrows them for its duration, so a recording is not tied to the cfg it was made under, and returns the player's own on the way out |
| A savegame at each end | Where the session started, and where it finished. Both inside the file |
| An FNV-1a digest of simulation state, per tick | Scene, hero, camera, the other actors, the open modal, and all 336 script variables |
| A keyframe of named state, every 32 ticks | The digest says *when* a replay stopped matching; this says *what* moved |
| Every value the digest mixes, per tick, with `--record-telemetry` or `rec start verbose` | The keyframe names 23 fields. This names all of them, so a divergence in another actor or a script variable is named too |
| The settings a replay is known to turn on | They are not in the save, and a config edited in between reads as the simulation diverging |
| The mode the session ran in, audio included | Whether a sample driver came up decides what the simulation computes, and no save or config records it |

The header is `key=value` text, so a recording is readable without a decoder. `SOURCES/RECORD_FORMAT.H`
owns the field readers, the binding-table lines and the frame around an inline savegame;
`tests/record_format` covers them.

## One file

A recording is a single `.rec`. A stream with two savegames beside it would be a recording only
while nothing separated them, and nothing keeps a set of files together: copy one and leave the
others, and the replay reloads from somewhere the session never was. The savegames are 4 to 17 KB
against a stream of megabytes, so carrying them costs nothing worth that.

They are framed so a run that dies mid-write leaves a file that still reads up to the last thing it
finished writing:

```
LBA2REC 11 + key=value header lines, ending in a blank line
[0x70][u32 len][savegame][u32 len][magic]   the state the session started from
... polls, ticks, keyframes, telemetry, commands, sync markers, flushed every tick ...
[0x71][u32 len][savegame][u32 len][magic]   the state it ended at
```

Three properties do the work, and each of them is a rule about the *writer*:

- **Nothing is written that has to be corrected later.** Both savegames exist whole before their
  chunk is written, so the length goes down ahead of the payload and is never revisited. A file cut
  part way through a chunk is short of its tail; it never claims bytes that are not there.
- **The tail is the length again, then a magic word.** This is the check the frame exists for. A
  half-written savegame is a valid savegame right up to where it stops, and the save loader would
  take it: the replay would then start from a state the session never reached, and report a
  divergence with no cause in it. A chunk that does not close is refused by name instead.
- **The end savegame is a trailer, so its absence still means what it meant.** `rec stop` writes it
  only while a scene is live, so a run that crashed carries no end state, and that is the record of
  the session not having finished. Everything before it still replays.

Measured against real truncations of a real recording: cut inside the start savegame, the replay
says so and checks nothing; cut mid-session, it replays the 125 ticks that reached the disk; cut
inside the end savegame, it replays all 298 and stops there.

The savegames come back out with `scripts/dev/dump_recording.py session.rec saves`, which writes
`session.start.lba` and `session.end.lba` and needs no engine.

The one place a snapshot still touches the filesystem is on its way past: the engine's save layer
works in paths at both ends, so `rec start` stages the savegame beside the recording as
`<name>.staging.lba` and a replay that reloads stages it back. Removed once the load has read it,
and named after the recording rather than shared, so two engines on one user directory cannot
overwrite each other's starting state. Not the save folder, which is the obvious place and the
wrong one -- the load menu lists every `.LBA` it finds there, and a staging file would show up as a
save nobody made.

A recording in an older format carries neither savegame and names a sibling in `setup.snapshot=`.
Both readers take that path too, which is why the oldest format the build reads is older than the
one it writes: those are sessions somebody played, and there is no second run of them to replace
them with.

`scripts/dev/dump_recording.py` reads the record stream without the engine, which is how to look at
a session the engine cannot be run against. Its keyframe output is a delta per line, so a session
reads as what changed:

```
kf  1600: cube 60->35  cubemode 1->0  hero.x 19483->4078  blackpal 1->0
kf  1728: dial.obj 0->2
```

`analog` reports the polls that carried a mouse or a stick, which is the one part of a session
that reads as nothing at all when it is missing:

```
$ scripts/dev/dump_recording.py session.rec analog
poll 42 rsx 0 rsy 0 padfirst 0 mdx 20 mdy 0 click 2
```

## Replaying on another platform

A recording made on Linux replays on Windows, and one made on Windows replays on Linux. One file,
no conversion.

That rests on the engine carrying its own random number generator
(`LIB386/SYSTEM/RANDOM.CPP`), because libc's `rand()` is not one function:

| | `srand(0)` first draws | `RAND_MAX` |
|---|---|---|
| glibc (Linux) | 1804289383, 846930886, 1681692777 | 2147483647 |
| UCRT (Windows) | 38, 7719, 21238 | 32767 |

Different order, and a different range, so `rand() % n` is differently distributed as well as
differently ordered. One shared stream feeds rain, ambient sound, animated textures, particle
scatter, track AI and the `LF_RND` script opcode, so drawing from libc means two platforms part
company within a few ticks of any scene with something moving in it.

The generator reproduces glibc's, specifically, and that is the choice that costs nothing: every
committed baseline, corpus save and reference recording in this repository was made on Linux, so
matching glibc leaves all of them describing the same engine while putting every other platform on
it too. It is not the retail sequence, which was Watcom's and which no port of this engine
reproduces.

### What a recording declares about arithmetic

Two header lines say what a replay has to agree with, and they name traits rather than platforms,
because the platform is not what decides it:

| Line | Meaning |
|---|---|
| `numeric.rng` | Which generator `Rnd` draws from. A recording made against another one cannot reproduce. |
| `numeric.long_double_bits` | `LDBL_MANT_DIG`. 64 on both x86-64 hosts, which is why they agree on projection and distance. |

`build.platform` is recorded too, but deliberately not compared: a cross-platform replay is the
point, and a diff that flagged the platform every time would teach a reader to ignore the report.

The second line is there ahead of needing it. `LIB386/3D` computes projection, rotation and
distance in `long double` and rounds with `lrintl`, chosen to match x87's 80-bit registers. Both
x86-64 hosts have those registers, so Linux and Windows agree. ARM does not, so on macOS and
Android `long double` is a plain `double` and the same expressions round differently. A recording
from an x86-64 host will not reproduce there yet, and because the trait is in the header, such a
replay opens by naming the arithmetic it disagrees about instead of diverging in the middle and
reading like a bug in the simulation. Making that case replay means taking those paths off the
host's extended precision, which is a separate and larger job.

### Checking it

`tests/random` is the oracle, and it is two oracles doing different jobs. A committed vector of
draws runs on every platform and is what actually pins portability: reproduce it and you draw the
same numbers, whatever the local libc does. A live comparison against the system `rand()` builds
only on glibc, and checks the other half, that the committed vector really is glibc's sequence
rather than one agreed among ourselves. It is compiled out elsewhere rather than skipped at
runtime, because a skipped check exits 0 and reads exactly like a passing one.

## Verbose telemetry

A plain recording carries one 64-bit digest a tick. That is enough to detect a divergence and can
never explain one: roughly 1300 values go into it and the digest names none of them. The keyframe
narrows that to 23 named fields every 32 ticks, which covers the hero, the camera and the open
modal, and covers no other actor and none of the 336 script variables.

`--record-telemetry` on the recording run, or `rec start verbose` on the command, stores every
value the digest mixes, every tick, so the first rejected tick names the fields that moved:

```
[rec] verbose telemetry: 668 values a tick
[rec] consistency failure at tick 201: recorded 2060ee23…, replayed ab660bc5…
[rec] tick 201 state differs: var.game[77] 0/42  (recorded/replayed)
```

The names come from the expression the digest mixes, so a field is hashed and reported under the
same name: `hero->Obj.LastFrame`, `obj[12].Anim`, `var.cube[7]`. A field added to the digest is
named without anyone having to name it.

The first rejected tick is reported along with the two after it, which is usually enough to tell a
value that moved once from one that is drifting. About 2.8 KB a tick -- measured at 2787 bytes a
tick over a 198-tick session -- so it is recorded deliberately rather than by default: roughly 5 MB
for a session of a few thousand ticks, and about 240 MB of telemetry alone on a half-hour one.

The bytes are the cost, and the time is not: writing them adds 0.10 ms of CPU a tick, measured
against the same build without them, which is 0.6% of a 16 ms step. System time does not move --
the per-tick flush is absorbed by the page cache, and the 0.10 ms is the loop that marshals the
values rather than the write. So the question of whether to record it is about the size of the file
somebody has to keep or send, and not about what the run does while it is being made.

The two ways of asking differ in when you can ask. `--record-telemetry` is a decision made before
the run starts, which is the wrong moment for a bug you have just watched happen: relaunching to arm
the flag costs you the session that showed it. `verbose` on the command arms the same telemetry from
wherever you are, and the header records which recordings carry it, so `rec info` on a file from
last week still says. Only the recording run needs either: a replay reads the values out of the
file, so passing the flag to a replay arms nothing. Nothing else about the recording changes -- telemetry is written beside the
digest and read back by the replay's reporter, and never reaches the simulation.

## Limits worth knowing before you trust a result

**Give the replay more ticks than the recording holds.** Ending on `--tick` before the stream runs
out means the summary never prints, and a run that reported nothing reads exactly like a run that
passed. A replay says how much the file holds as it starts, so the number does not have to be
guessed:

```
[rec] holds about 3699 ticks over 134410 polls; give --tick more than that
```

**A replay needs the same `--load` the recording ran under.** For a recording made with a
boot-time `--load` that is unsurprising: `setup.reloaded` records whether the session reloaded its
own snapshot, a `--load` at boot is not that, and so nothing in the file restores the start state.

A mid-session `rec start` does reload its own snapshot, and still needs it. Measured: the same
mid-session recording replays with no mismatch when the replay is given the same `--load`, and
diverges at tick 4 without it, from both `--replay` and `rec play`. So something the reload does
not restore differs between a run that booted into the save and one that booted fresh, and a
mid-session recording is not yet self-contained either. Naming the save the session started from is
the outstanding work; until then, pass it.

**Some settings have to match, and they are not in the save.** A replay reads the live config, so a
config edited between recording and replay reads as the simulation diverging. Each of these was
found by changing one cfg key at a time on the replay side of a recorded session:

| Setting | Diverges at | Why |
|---|---|---|
| `FollowCamera` | tick 0 | The camera is in the digest |
| `FollowCamGroundClearance` | tick 2 | The camera settles at a different distance |
| `DetailLevel` 3 to 0 | tick 235 | `SetDetailLevel` turns `RainEnable` off, and rain draws from the one shared `rand()` stream |
| `MouseCamera` 1 to 0 | tick 61 | The recorded motion is discarded instead of turning the camera |
| `MouseCameraDrag` 1 to 0 | tick 61 | Orbits on motion the recording made with no button held |
| `MouseSensitivityX` 4 to 8 | tick 61 | The recording stores raw motion; the replay scales it |
| `MouseCameraDivisor` 4 to 2 | tick 61 | The other half of that scaling |
| `MouseSensitivityY` 4 to 8 | tick 131 | Same, on the first gesture with a Y component |
| `MouseInvertY` 0 to 1 | tick 131 | Same motion, opposite sign |
| `FollowCamOrbitGlide` 75 to 20 | tick 101 | Where a gesture ends and the glide takes over |
| `GamepadCameraAnalog` 1 to 0 | tick 61 | The recorded axes are not read at all |
| `GamepadCameraSensX` 5 to 9 | tick 61 | Same deflection, different orbit speed |
| `GamepadCamMaxBeta` 36 to 72 | tick 61 | The ceiling that speed scales to |
| `GamepadDeadzone` 8000 to 12000 | tick 61 | Decides whether a deflection registers |
| `GamepadCameraSensY` 5 to 9 | tick 131 | Same, on the gesture with a Y component |
| `GamepadCameraInvertY` 0 to 1 | tick 131 | Same deflection, opposite sign |
| `GamepadCamMaxAlpha` 10 to 20 | tick 131 | The elevation ceiling |

The analog rows needed a session that orbits. Two earlier sweeps drove the keyboard and returned a
column of "none" for all of them, which is why the tick each one fires on is worth reading: it is
the gesture that setting shapes, not an arbitrary point in a session that would diverge anyway. The
mouse and stick rows split by axis in exactly the same way, X keys on the X gesture and Y keys on
the first gesture with a Y component.

`GamepadDeadzone` is the widest of the set and the likeliest to differ between two players, being
what a stick with drift gets tuned on. It is not only a camera setting: `SOURCES/JOYSTICK.CPP`
applies it to the left stick's axis-to-scancode conversion too, so it decides what a recorded
deflection means for movement as well.

The gamepad keys are read in `SOURCES/INPUT.CPP` rather than `SOURCES/CONFIG_FILE.CPP`, which is
worth knowing before concluding that a sweep has seen the whole config.

The header carries all of these, and a replay names the ones that differ as it starts, by the key
the player would edit:

```
[rec] mode differs: settings.MouseSensitivityX=4 (this run: 8)
[rec] 1 mode line(s) differ; this replay may not reproduce
```

Not carried, and the distinction matters: `Shadow` is inert because `SetDetailLevel` overwrites it
at MainLoop entry, while `AllCameras`, `ManualCameraSmoothing` and `FlagDisplayText` have only read
clean on the sessions swept so far. `ManualCameraSmoothing` needs an orbit that leaves the camera
far enough from its target to take the stepped branch in `SOURCES/FOLLOWCAM.CPP`, and
`FlagDisplayText` needs a dialogue, which none of these sessions has. The boundary is empirical
rather than principled: a setting joins the block when a replay is shown to turn on it.

**The console is not recorded, and used to take the clock with it.** Its toggle arrives as an
SDL key event rather than through the polled key table the recorder samples, so a replay never
opens it. Two consequences, both found by replaying a real session and both now fixed:

- Its every-frame redraw was a second present, and under `--fixed-dt` every present past the first
  steps the virtual clock, so the game clock ran at double rate while the console was open. The
  redraw is now marked as an overlay ([TIMING.md](TIMING.md)).
- The keys typed into it were recorded as gameplay input, because the recorder samples upstream of
  where `SOURCES/INPUT.CPP` clears them. Typing `exit` put scancodes 8, 12, 23, 27 and 40 into the
  file, two of them bound to actions in that player's config, and a replay with no console open
  would have pressed them into the game. The recorder now records what gameplay is given, which is
  nothing while the console is up.

- A command typed at the console is written where it ran, which is the input path, so it lands
  after the last poll of the tick and before that tick's own record. The reader expected commands
  only after the tick record, gave up on the tick when it found one, and then read the tick record
  as a poll: a replay ended on the first command a player typed. Both readers now step over
  commands and run them at the tick boundary.

Recordings made before those fixes still diverge where the console was opened: the doubled clock is
in the file, and no replay reproduces it.

Worth knowing about the coverage: `--exec-at` writes a command straight after the tick record, so
the automation fixture exercises the harness layout and not this one. The console layout cannot be
produced headlessly, because the toggle is an SDL key event. It is covered by a real recorded
session that opens the console, types, closes it and plays on, which replays with no mismatch and
no clock drift.

**Audio used to move the answer, and that is now fixed at the source.** The ambience system draws
its random pan from the one `rand()` stream actor behaviour uses, and only `if
(!IsSamplePlaying(...))`. The SDL mixer answers that from the audio device's clock, which no replay
reproduces, so one draw either way offset the stream and the next actor to reach a "turn to a random
angle" instruction turned differently. Measured: a walking session diverged at tick 1410 on
`obj[11].Beta`, with zero clock drift, and the same walk recorded `--no-audio` replayed clean.

A sample's playing state is answered from its own length on the game clock rather than from the
mixer for any run that has to reproduce, which is a pinned run and also a recording or a replay on
a host-sampled one. The loose path needs it more, having nothing else holding its two ends
together. The mixer is untouched: this only changes what the simulation is told.

Measured on the loose path, audio on: one recording replayed five times went from clean on some
runs and diverging on others to clean on all five, and a sweep of eight fresh record-and-replay
pairs went from none clean to three.

This is also why the bug survived so long. Every fixture runs `--headless`, which skips
`InitSampleDriver` altogether, so nothing in the suite had ever exercised the audio path. A
recording fixture is the only one that runs with a sample device up.

Skipping the driver is not the same as building the null backend, and the difference matters to
anything asking whether a run had audio. `--headless` leaves `Sample_Driver_Enabled` false;
`SOUND_BACKEND=null` initialises, sets it true, and then returns `FALSE` from every
`IsSamplePlaying`. Ask `Sample_DriverPlaysSound()` instead, which each backend answers for itself.

**Time spent in a modal is not checked.** The digest fires once per tick, and a modal that waits
for input spins polls without advancing one: a recorded session that sat in a dialogue choice held
707,624 polls against 1,472 ticks, so roughly 580,000 of those polls had no oracle behind them at
all. "1472 ticks checked, mismatch -1" means the simulation matched on every tick it took. It says
nothing about the forty seconds spent in the menu, and a replay that behaved differently in there
would still report clean.

**The mode has to match, and audio is the half of it that is not fixed at the source.** The fix
above makes two runs that both have audio agree. It does nothing for a recording made with sound and
replayed without it, because `--no-audio` skips `InitSampleDriver` entirely: `IsSamplePlaying` is
then an unconditional no rather than a differently timed yes, which is a different branch and not a
quieter one. It reaches further than the ambience draw, too. A dialogue with the text off spins on
`TestSpeak()` until the voice sample ends, so the same line holds the game for hundreds of ticks in
one run and none in the other. `--headless` implies `--no-audio`, so every replay driven that way is
silent.

A replay does not have to be driven that way, though, and since the recorder pins its own step there
is no longer a flag pulling it there either: `--fixed-dt` sets the control harness active but does
not require `--headless`, and a mid-session `rec start` needs no flag at all. Measured, with a sample
device up on both ends and neither run headless or given a step -- `rec start`, `rec stop`, then
`rec play` in a second run -- the recording carries `mode.audio=1` and replays at `first hash
mismatch -1` with no mode line differing. So a session recorded with sound is reproducible by a
session replayed with sound; what stays broken is mixing the two, which is what the line reports.
That round trip has no fixture behind it, because the arms that could run it are the ones that have
to skip where SDL will not open a dummy device.

So the header carries `mode.audio`, and a replay names it as it starts:

```
[rec] mode differs: mode.audio=1 (this run: 0)
```

It records whether a sample driver actually came up rather than which flag was passed, because
`--no-audio`, `--headless` and a host whose audio device fails to open all land in the same place
and the last of those is nobody's choice. A recording written before that line existed carries no
answer, and a replay compares only the lines the file has.

**Recording from boot headlessly needs `--exec "skipmodals 1"`.** The opening dialogue waits for a
keypress that a headless run never sends.

**A harness-driven recording carries its own commands, so the sample stream is not its only
carrier.** A replay re-executes every console command the session was driven with, which for a
fixture means `key`, `input` or `mouse` reproduces the input a second time and would cover for a
sample stream that had stopped working. A session someone played has no commands behind it, so
there the samples are all there is. What tells the two apart is a replay run driven the other way
at the same time: the file has to win, and `test_record_analog.sh` is built on that.

**A replay reports a pad present on the polls that carry a deflected stick.** The analog camera
asks whether a pad is there before it reads the axes, so restoring the axes alone leaves a pad
session replaying on a pad-less machine with the values put back and the code that reads them
skipped. Presence is taken from the axes rather than from a header field, because the header is
written before the session starts and the only thing gated on presence does nothing with a centred
stick: a poll with no deflection and a poll with no pad behind it are the same poll as far as the
game is concerned.

## Testing

| Layer | What it covers |
|---|---|
| `tests/record_format` | Header field lookup and binding round trips. Host test, no retail data, runs in CI |
| `tests/automation/test_record_replay.sh` | A real engine records and replays, with and without a tick budget, and with verbose telemetry. Needs retail data, so it does not run in CI |
| `tests/automation/test_record_analog.sh` | The mouse and the stick round trip: the file carries the input at the poll it happened, and beats a device moving under the replay. Needs retail data and the exterior corpus save |

The telemetry arm changes a game variable part-way through a replay and requires the report to name
it. A reporter that printed nothing would pass a clean-replay check exactly like one that works, and
this is a diagnostic whose only job is to be believed on the day something real diverges. The
console form is checked from the other end, on the file rather than on the report: one session
recorded with `rec start verbose` and one without, both read back with no engine in the loop, and
the second is what says the first proves anything.

## Diagnosing a divergence

A mismatch reports the tick, and the next keyframe reports the fields:

```
[rec] consistency failure at tick 101: recorded 782c3059…, replayed d69a8ed8…
[rec] tick 128 differs: hero.anim 0/1  (recorded/replayed)
```

The keyframe covers the hero, the camera and the open modal, which is most of what goes wrong. When
what moved is outside it (another actor, or a script variable), a recording made with
`--record-telemetry` or `rec start verbose` names it directly and the rest of this section is
unnecessary. Without one,
the tick is still the starting point:

1. **Compare the recorded clock, not just the hashes.** Each tick record carries `TimerRefHR`
   alongside the digest. Dumping both from each end and putting them side by side distinguishes "the
   state diverged" from "one end reached this tick a step later", and the second is far more common.
   Hashes can stay identical for a dozen ticks after the clocks part.
2. **`--dump-state` either side of the tick, and diff the JSON.** That turns a digest into a field
   name.
3. **`lifetrace` / `objtrace` on the actor that differs**, to find which script or animation moved
   it.

Two traps this has already cost time on: a control run must extend past the divergence tick to mean
anything, and a load re-derives state stamped from the clock, so both ends looking identical
immediately after a load says nothing about a few hundred ticks later.
