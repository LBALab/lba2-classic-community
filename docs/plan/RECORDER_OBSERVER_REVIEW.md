# The recorder as an observer: architecture review

An architecture review of the session recorder against what it is for: an oracle a refactor can be
checked against, a bug report a player can attach, a fixture that carries its own assertion, and a
session a player can replay themselves. Judged on three properties those uses need -- minimal impact
on the session observed, telemetry good enough to explain a failure, and a file that travels. It
proposes an ordering and commits nothing.

Pinned to `c00a0406`, which was `origin/main` when it was written. Merged work since then is cited
by number and SHA wherever it bears on a conclusion, and the count is deliberately not stated, since
a number in this position is the first thing here to go stale. **#621** (`5de09158`) put the telemetry cost figures into
[RECORDING.md](../RECORDING.md), so they are cited from a file rather than a session. **#622**
(`4aaf7bd1`) taught `dump_recording.py` to read telemetry, clock and analog offline. **#624**
(`eb786441`) is discussed below as the clearest example of the fix shape this review recommends. **#625**
(`32a8db9f`) closed two of this document's own sequencing items while it was being written; they are
struck through rather than deleted, so the ordering argument stays readable. **#626** (`79981ad5`)
taught the offline reader to name digest version 2. **#623** (`c7617da8`) closed the
`--verbose` double duty discussed below. Later still, **#630** consolidated the clock pacing,
**#635** and **#636** (`8b34304c`) closed the loose-clock fade wedge, and **#637** (`952e77b3`) and
**#638** (`51fe3890`) closed the reproduction-rate causes named in the sequencing. Open work is
named with its number and state where it changes a conclusion; nothing here depends on any of it
landing.

Line numbers were read from the working tree at that commit. Measurements marked *(measured
elsewhere)* were made by another session against the same commit and are cited rather than
re-derived. Everything asserted about the source was read here. A set of figures that circulated
during this review has since been withdrawn and is listed at the end, so nobody re-derives them.

## Why the recorder exists

Worth restating, because two of the frictions below are the price of a use nobody is currently
optimising for, and one of them is the price of a use that has not arrived yet.

**It is the oracle a refactor has never had.** A pure refactor is one that leaves the simulation
identical. A corpus of recordings replayed against a new build says exactly that, per tick, by
field name. [ARCHITECTURE_GLOBALS.md](../ARCHITECTURE_GLOBALS.md)'s bounded-context work is
precisely the change that should be invisible to every recording, and until the recorder existed
there was no way to say so except by argument. This is the use that pays for the format work, and
it is the one that wants the digest to be *complete* more than it wants the recorder to be
comfortable.

**It is a bug report a player can attach.** One file, self-describing, that says at a named tick
when it did not reproduce on the maintainer's machine rather than looking like it did. This use
wants portability and honesty, and it is the one that makes a plausible wrong answer the worst
failure mode there is.

**It is a fixture that carries its own assertion.** 62 control-harness fixtures are a script plus a
separately-maintained expectation. A recording is the same thing recorded, and the expectation
cannot drift from the script because it is the same file.

**And the bonus: a player can replay their own session.** This is the one that turns every design
choice above into a constraint. It has to work with no flags and no paths, from where the player
already is, on the machine and the config they already have, and it has to give the session back
afterwards. `rec start` / `rec stop` / `rec play` is the right shape for it, and the mid-session
path is genuinely reachable today.

## The finding

**The recorder is three things wearing one name, and only the first can be passive.**

| Role | What it does | Can it be passive? |
|---|---|---|
| Observer | Samples the poll at `UpdateKeyboardState`, digests state per tick, writes the file | Yes, and it nearly is |
| Scheduler | Pins the simulation step, then paces frames with `SDL_Delay` to undo the pinning | No, by construction |
| Session manager | Writes a snapshot, reloads it, holds a return point, swaps the binding tables | No, by construction |

The observer half is well built. Four hooks, three of them weak symbols so `LIB386` carries no
dependency on `SOURCES` ([KEYBOARD.CPP:60](../../LIB386/SYSTEM/KEYBOARD.CPP#L60),
[TIMER.CPP:269](../../LIB386/SYSTEM/TIMER.CPP#L269),
[CONSOLE.CPP:1617](../../SOURCES/CONSOLE/CONSOLE.CPP#L1617)), one function wide at the digital
waist, a single self-framing file, and a numeric contract in the header. That is a good design and
this review does not propose changing it.

The other two roles are borrowed, and **every friction on the list below is one of them showing**.
The recorder took them because the engine has no simulation tick and no notion of a resumable
simulation state. It is not that the recorder reached too far; there was nothing there to hold on
to.

The practical consequence: **stop trying to make the recorder passive by improving the recorder.**
Name each borrowed responsibility and give them back one at a time. One of the three is already half-returned, and the
measurement behind it says the trade the recorder was built on may not have been necessary.

## The failure mode is plausible wrong answers

Four measured findings this review was nearly built on were artefacts. Before PR #616, a replay with
no game to replay into did not refuse: it diverged, reported a tick, and nothing in its output
distinguished "the simulation diverged" from "there was no simulation to diverge from". A replay arm
that omitted `--load` produced a constant clock offset, an audio figure, an optimisation-level figure
and a cross-platform figure, and each was the missing start state wearing the costume of a result.
They are listed at the end so nobody re-derives them.

**The recorder's value is entirely in being believed, so its failure modes have to be
distinguishable from its findings.** Every other quality -- portability, low overhead, a compact
file -- is worth nothing on a tool that answers confidently when it does not know. The general form
of that, with the other instances it turned up across the engine, is in
[BUG_HUNTING.md](../BUG_HUNTING.md) under "Oracle discipline"; what follows is the part specific to
this recorder.

The rule: **a replay that cannot establish its own preconditions must refuse and name the
precondition, never proceed and report a tick.** #616 closed one such case. Three more are
open or were open recently.

**The settings comparison is a reporter where it needs to be a gate.** A replay into a run whose
settings do not match still runs to completion:

```
[rec] 6 mode line(s) differ; this replay may not reproduce
[rec] consistency failure at tick 0: recorded 36767d57..., replayed 20723fe5...
[rec] replay ended at poll 901: 901 ticks checked, first hash mismatch 0, clock drift max 0 ms
```

Exit code 0, and the broken precondition was detected and named *before* the run started. Read
quickly, "first hash mismatch 0" is a simulation result; it is a settings mismatch the tool spotted
and continued past. The cause is structural: `replay_report_mode`
([RECORD.CPP:1540](../../SOURCES/RECORD.CPP#L1540)) is `static void`, so it counts differences into
a local, prints them, and returns nothing its sole caller ([:1998](../../SOURCES/RECORD.CPP#L1998))
could act on.

The scope is smaller than "decide which lines are fatal", because half is already decided.
`mode_line_ignored` ([RECORD.CPP:1511](../../SOURCES/RECORD.CPP#L1511)) is the advisory set with a
reason against each entry: the clock baseline is restored rather than matched, the binding payload
is installed rather than compared, `build.platform` differing is the whole point of a cross-platform
replay, `mode.verbose` never reaches the simulation. The fatal half is probably **`settings.*`
alone** -- those lines come from `Control_DescribeState`
([CONTROL.CPP:1847](../../SOURCES/CONTROL.CPP#L1847)) under one prefix, so the gate is a prefix test
symmetric with the ignore list, and it never reaches `build.platform`. The settings block is
simulation-affecting by construction: every member earned its place by a measured divergence.

**A replay still needs the `--load` the recording ran under.** [RECORDING.md](../RECORDING.md)
documents it, including for a mid-session recording that reloads its own snapshot, and the research
doc names making a recording self-contained as the highest-value open item. This review adds a
reason: it is the precondition whose absence produced four false findings in one afternoon. Until it
closes, the refusal is the safety rail, and the refusal is newer than most of the recordings in the
tree.

**A truncated run reads like a passing one.** A 90,692-tick contributed recording, replayed without
its preconditions met, stopped at recorded tick 36 and exited cleanly with no `replay ended` line at
all. That is [RECORDING.md](../RECORDING.md)'s documented limit firing on a real file. The session
that hit it was careful enough not to report 36-of-90,692 as a divergence; the next person will not
be. A replay that ends without printing its summary should say why.

**And the report caps mean a divergence can only ever name 24 values** -- 3 keyframes
([RECORD.CPP:2527](../../SOURCES/RECORD.CPP#L2527)), 3 telemetry ticks (:2561), 8 fields shown
(:2582). A broad divergence and one stale flag produce reports that read alike, and telling those
apart is the whole job on the day something real diverges. Keep the console cap; lift it on the
file. That is sequencing item 4, and it is currently blocking a feature rather than merely annoying.

### What a verdict is worth, and what to repeat

Both ends of a loose recording move, and they were established one at a time in a way worth reading
as a caution about K.

A first measurement found that any single loose recording replays to the same verdict every time --
one file clean 10/10, one mismatching at tick 1 10/10, one at tick 235 10/10 -- while two recordings
from an identical command line differ. That supports "it is the recording that varies, not the
replay", and it is wrong as a general claim. A wider design retired it: **K=8 fresh recordings, each
replayed twice, found the two replays disagreeing on 2 of 8 files in one arm and 3 of 8 in another**
*(measured elsewhere)*. Replays do disagree, on roughly a third of files. The first result was three
files that happened to be stable, which is what a small K looks like when it is lying.

So a loose corpus needs **both**: K fresh recordings, and each replayed more than once before
acceptance. Recording once and replaying once measures neither variable; recording once and
replaying K times measures the weaker one.

**Both variables were already written down, in one sentence, and every reader took half.**
[RECORDING.md](../RECORDING.md) records that the ambience fix took "one recording replayed five
times ... from clean on some runs and diverging on others to clean on all five, and a sweep of eight
fresh record-and-replay pairs ... from none clean to three." First clause replay-side, second clause
per-recording, and separate readers cited each without noticing the other. The tidy reading -- that
the first clause describes a defect since fixed -- is also wrong, and contradicted by the
disagreement column above. What it establishes is that the fix helped *that one recording*, at n=5.
A single clean result establishes *can*, never *how often*.

### The rate has a named cause, and it is not per-frame state

Everything `--fixed-dt` has been found to mask until now was frame-timing state: the sub-step carry,
the hero's animation anchor, the audio gate. The reproduction rate has a different cause, and it is
a single line.

`ChangeCube` reseeds the engine's one RNG stream from the game clock:
`Rnd_Seed(Demo_RngSeed(DemoSlide, TimerRefHR, NewCube))`
([OBJECT.CPP:1194](../../SOURCES/OBJECT.CPP#L1194)), and `Demo_RngSeed`
([DEMO_SEED.CPP:7](../../SOURCES/DEMO_SEED.CPP#L7)) returns the cube number under `DemoSlide` and
**`TimerRefHR` otherwise**. On a boot `--load` the cube change runs before the savegame's clock
lands, so the seed is the process's own boot duration in milliseconds. Measured across replays of one
byte-identical file: seeds of 44, 48, 49, 50, 52, 54, 58, 63, 70 and 75 *(measured elsewhere)*.

Three things follow, all from the same instrument:

- **The seed decides the verdict, deterministically.** 16 replays of one file under load, seed and
  verdict read from the same run: seed 44 gives a mismatch at tick 159, seed 48 replays clean, seed
  49 mismatches at tick 5, each three times out of three. No seed produced two verdicts.
- **The rate is boot jitter.** One fixed recording, N=24 a side: 1 of 24 diverged idle, 14 of 24
  with six spinners on six cores.
- **`--fixed-dt` pins the seed to zero**, because `Timer_EnableFixedDt` zeroes `TimerRefHR`. That is
  why a pinned replay is exact, and why this appeared in no pinned measurement.

The mechanism was already written down. [plan/RECORDING_RESEARCH.md](RECORDING_RESEARCH.md) records
that `ChangeCube` "is the engine's only `srand` and seeds from `TimerRefHR` outside the demo reel,
which is what makes a cube change anywhere in a session clock-sensitive". What had not been
connected is that this *is* the reproduction rate rather than one contributor among several. A fact
being documented is not the same as it being read, and this is the largest instance of it here.

**And the RNG stream is in none of the three categories above**, which is what let it sit unnoticed:
`rng.draws` is deliberately reported rather than hashed, the seed is carried nowhere, and no
savegame restores either. The membership rule would have raised it as a question the first time
anyone applied it.

The fix shape exists in the tree for the sibling case. `SOURCES/RECORD.CPP` says of the mid-session
path that two ends reaching the cube change at different readings draw different streams, and that
pinning the clock before the load settles it at its cause. The boot `--record` and `--replay` path
has no equivalent, and one constraint rules out the obvious version of it: **a replay cannot ask the
file for the seed at the moment the seed is used.** The recorder arms on its first input poll, which
is after the boot load, and by then the stream is seeded and drawn from. No later correction reaches
it, so the one field has to be read where the flag is parsed, ahead of the load.

That is the shape #637 took (`952e77b3`). The header carries `clock.rng_seed`, `--replay` peeks that single
field at arm time while everything else still waits for the recording to be opened properly, and a
seed hook hands it to `ChangeCube` for the first load only; every later cube change seeds from the
clock the savegame restored, which both ends already agree on. Additive, so a file without the field
replays exactly as before.

The mechanism was re-derived from the source independently rather than taken from here, and
instrumented at the seed site: 14 replays of one byte-identical file, seed 40 -- the recording's own
-- clean 11 times of 11, seed 41 diverging at tick 225 three times of three, no seed giving two
answers. Twelve fresh recordings replayed twice each move the reproduced column from 4 of 12 to 10
of 12, and **the disagreement column from 5 of 12 to zero**, Fisher two-sided p = 0.036 and 0.037
*(measured elsewhere)*. The disagreement column is the stronger of the two, and it is the one to
watch on any measurement of this kind: a replay takes its input, its clock, its step and its
bindings from the file, so two replays of one fixed file disagreeing says it is reading something
the file does not carry. That is a bounded search rather than an open one.

### What is left of the rate is a delta the replay throws away

With the seed carried, 1 recording of 16 still fails, the same file giving the same verdict twice.
The digest names `obj[1].LastTimer`, `obj[1].NextTimer` and `sim.carry` -- an actor's animation
anchors and the sub-step carry, all **one millisecond apart**, which is one animation sub-step on
the first actor stamped *(measured elsewhere)*. So the residual is a real divergence rather than a
digest reading state nothing carries, and the field names are what settled which: none of the five
globals named below appears in it.

The cause is on the reading side. A recording banks the interval between the reading `record_begin`
takes its baseline from and the reading the first `record_poll` holds, and writes that down as the
first poll's delta. The replay adds that delta to `s_replaySrc`
([RECORD.CPP:1947](../../SOURCES/RECORD.CPP#L1947)) and then calls `SetTimerHR` with the baseline
five lines later, which assigns straight over it. The two ends part by exactly that interval, and
the interval is only ever non-zero while the clock is live. Installing the baseline before the
advance, so the replay banks the same first interval the recording did, is #638 (`51fe3890`).

**Two hypotheses died on the way, and why each was reached transfers further than the answer
does.** The first is that the header's declared baseline is stale
relative to the recording's own first tick, which is the natural reading of a failing file whose
header sits a millisecond under the digest that failed. It came from comparing the header against
`sim.carry`, which is a different field, and a trace over 20 runs killed it -- the
header value and the recording's own tick-1 `TimerRefHR` are equal every time, on clean and failing
files alike *(measured elsewhere)*. The second is that this was a digest-membership defect, killed
by the field names above.

Both wrong readings put the fault in the writer, and one line explains why. `record_begin` carries
a comment describing this exact failure ([RECORD.CPP:1146](../../SOURCES/RECORD.CPP#L1146))
-- the baseline "has to be the clock the first recorded frame runs on, not the one the frame arrived
with" -- and that comment is correct, closed, and about the other end. **A comment that names the
failure mode you are chasing is a magnet.** It reads as a confession and is more often a record of
someone having already handled that side.

**The clock-live shape holds even though the cause moved.** The seed is a constant substituted for a
variable, since `Timer_EnableFixedDt` zeroes `TimerRefHR` and every pinned run therefore seeds with
0; this is an interval that only accrues while time passes between two samples. Neither is per-frame
state and neither can appear in a pinned measurement, which is the whole of why they went unfound
for as long as they did. The list of what `--fixed-dt` masks is not a list of per-frame timing
state. It is a list of everything that is a race only when the clock is live.

**The before/after could not be measured as a rate, and forcing the precondition is what made it
readable.** A straight comparison at K=25 gave 1 failure against 1, because the fault needs the
first poll to land about a millisecond late and that happens on roughly one recording in twenty.
Holding the first poll three milliseconds back, under a temporary env gate, turns the same number of
runs into a conditional: **15 of 15 failed before, 1 of 14 after** *(measured elsewhere)*. The
general form of that move is in [BUG_HUNTING.md](../BUG_HUNTING.md); the recorder-specific point is
that the natural rate then follows from how often the precondition holds and needs no large K at
all.

**A second unaccounted interval remains**, in that 1 of 14 and with the same millisecond-wide
signature. Recorded, not chased.

## The perturbation ledger

Everything the recorder changes about a live session, read at `c00a0406`. This is the list a
"minimal impact" claim has to be made against, and no doc currently holds it in one place.

| # | What | Where | Path |
|---|---|---|---|
| 1 | Pins the simulation step to 16 ms and zeroes `TimerRefHR` | [RECORD.CPP:1061](../../SOURCES/RECORD.CPP#L1061) | `rec start` |
| 2 | Paces frames with `SDL_Delay` to the tick boundary | [RECORD.CPP:2339](../../SOURCES/RECORD.CPP#L2339) | while pinned |
| 3 | Latches one clock reading per frame, so every `ManageTime` in the frame reads the poll's sample | [RECORD.CPP:2292](../../SOURCES/RECORD.CPP#L2292) | loose clock only |
| 4 | Writes a savegame and reloads the scene, mid-session | `record_reload_issue` | `rec start` |
| 5 | Marshals and flushes the tick record on the game thread | [RECORD.CPP:2397](../../SOURCES/RECORD.CPP#L2397) | always, while recording |
| 6 | Swaps `DefKeys` / `GamepadKeys` for the session's duration | `bindings_install` | replay only |

Which path a player is on is easy to get backwards. **A mid-session `rec start` always pins the
step itself** -- `record_reload_issue` arms
`Timer_EnableFixedDt` at 16 ms whenever the run did not already ask for one, and the comment there
explains why that is the only safe place: the call zeroes `TimerRefHR` and the load that follows
puts the clock back. So a player recording from the console is always on the paced path. The loose
path is `--record` from the CLI on a run that did not pass `--fixed-dt`, which is a developer path
and already warns at the moment of recording ([RECORD.CPP:1199](../../SOURCES/RECORD.CPP#L1199)).

Items 1 and 2 are what a player feels, and they are worth reading together: **2 exists to undo 1.**
Pinning the step makes game time advance per present rather than per wall clock, which measured
3.49x real time headless with no recording running at all, so the recorder inserts sleeps to put
real time back. A scheduler compensating for its own scheduler. The four rows under "Assert the
rate, not the state" below are the measurement, and they say something the ledger alone does not:
the pin is the perturbation, the observer is free, and the pacer succeeds on the main loop and does
not reach the modal loops. A playtest list *(measured elsewhere, from EPmager)* -- 60 fps lock,
transitions and menus racing, dialogue skipped -- is the "minimal impact on gameplay" principle
failing in the configuration a player actually records in, and its three symptoms are exactly the
places a modal presents without ticking.

Item 3 is the subtler one and the one to read twice. On the loose path the recorder replaces the
value the engine's own clock function returns, at roughly a hundred call sites, for the duration of
the recording. The comment at the site defends it as touching no mode flag, which is true and is
not the same claim as leaving behaviour unchanged. It also carries an explicit carve-out for
`FadeToPalAndSamples`, which is the shape of a hook that has to know about its host.

Item 5 is measured and fine, and the measurement is more interesting than the verdict. On the loose
path with no pacer, the extra cost of writing per-tick telemetry is 0.100 ms a tick *(measured
elsewhere: same three source files swapped against their own parent, CPU time, two tick counts per
binary to isolate 600 ticks, three reps, medians)* -- 0.6% of a 16 ms budget, and on the paced path
it is inside slack the pacer was going to sleep through anyway. Headless it reads as 9% more CPU
per tick, which is worst case by construction, since a headless tick is 1.07 ms with nothing
rendering. Two things worth keeping from it:

- **The cost is user time, not sys time.** The `write` and the per-tick `fflush` are absorbed by the
  page cache; the 0.1 ms is the marshalling loop of 578 `put32` calls. So anyone who wants this
  cheaper should look at the encoding, not the I/O.
- **The caveat travels with it.** Linux, warm page cache, local SSD. A slow or full disk, or a user
  directory on a network mount, could make the flush cost real in a way CPU time cannot see. The
  honest cost of the change is 2.1 MB per 900 ticks; the 0.1 ms is not the part to worry about.

### The invariant has no test, and the obvious test does not work

The research doc states the passivity requirement well -- "a recorder that perturbs the run it
observes produces recordings of a game nobody played" -- and describes the A/B that settles it: the
same scripted session with and without the recorder, state diffed. There is no such arm in
`tests/automation/test_record_replay.sh`. The nearest thing, at line 546, is a control run for a
different claim, that the recorded session moved at all.

An A/B written today would pass vacuously. Every record arm in that fixture passes `--fixed-dt 16`,
and under a pinned step `Record_ClockHook` returns before reading its latch, so the one ledger item
subtle enough to need a test is switched off in the mode the test would run in.

**And the loose-clock version of a state diff fails by construction, which is worth writing down
because it is the obvious repair and it is a trap.** Under a loose clock two runs of the same
scripted session do not reach the same state at all: game time is a function of how long each frame
actually took, so a control and a recording arm part company for reasons that have nothing to do
with recording. Such an arm would be flaky on its first day and would be "fixed" by pinning the
step, which is the exact thing it exists to rule out. The determinism the fixture currently enjoys
is itself a product of `--fixed-dt`.

**Assert the rate, not the state.** "Recording does not perturb gameplay" is a claim about speed,
and speed survives nondeterminism. Measured over 600 ticks with one `--exec-at` script, headless, and now on main in #625's commit
message rather than only in a transcript:

| Arm | Wall | Game time | Speed |
|---|---|---|---|
| control, loose | 2.73 s | 2.58 s | 0.94x real |
| record, loose | 2.76 s | 2.60 s | 0.94x real |
| control, pinned | 2.75 s | 9.58 s | **3.49x real** |
| record, pinned | 9.68 s | 9.58 s | 0.99x real |

So the arm is a ratio with a tolerance -- control against record, loose, within about 10% -- and it
is neither vacuous nor flaky. It asserts what row 2 shows, and it would have caught row 3 the day
that landed.

This is the cheapest item in this review and it is the guard that makes everything below safe to
attempt.

### What those four rows say about the ledger

They relocate the perturbation, and the third row is the one to read twice.

**The pin is the perturbation; the recorder is not.** Row 3 is `--fixed-dt 16` with no recording at
all, running the game at 3.49x real: 600 ticks are worth 9.58 s of game time where the natural run
makes them worth 2.58 s. Ledger item 1 does that on its own, before a byte is written. Row 2 says
the observer costs nothing measurable.

**The pacer works, and it covers ticks rather than presents.** Row 4 comes back to 0.99x, so
`SDL_Delay` does its job on the main loop. That has a consequence nobody had drawn: the reported
player symptoms cannot be main-loop pacing. The clock's advance points are *presents* --
`Timer_FixedDtPresent` in `BoxBlit` steps +dt for every present past a tick's first -- and the pacer
runs from the tick hook, which a modal inner loop never reaches. Fades, menus and dialogue are
exactly the places that present without ticking. So **the pacer covers ticks and not fades, menus or
dialogue**, and that gap predicts the three symptoms on the playtest list precisely.

State it as a mechanism, not a repro. It is real in the code and it has not been measured in a real
session, and one piece of evidence cuts against the simplest version of it: a 12,744-tick player
recording shows 16.5 ms a tick, so game time is not globally inflated. Whatever the modal gap costs,
it is not a uniform speed-up. This headless walking scene has no modals, which is why the suite has
never seen it either -- and is the argument for a fixture that opens one.

## Giving the scheduler back

The recorder pins the step because a recording has to be reproducible, and the explanation this
review was written against was that a host-sampled clock cannot be -- stated in
[RECORDING.md](../RECORDING.md) as the heading "The pinned step is required, not an optimisation".

That premise is false, by measurement: a loose recording reproduces `TimerRefHR` at 0 ms drift at
every tick, in every run measured. The clock reproduces fine. What does not reproduce is per-frame
state that nothing hands over, and the worked example is the one to put in front of anyone who
doubts the framing:

> `LastSimRefHR`, the #358 sub-step carry, was a `static U32` inside `MainLoop`. It decides where a
> frame's sub-step boundaries fall. It was in no digest, no save and no recording, so a replay
> started on whatever its own boot had left there.

Under `--fixed-dt` every frame is exactly 16 ms, `Timer_PlanSimSteps` always returns 1, and the
carry never matters. So:

**`--fixed-dt` is a masking mechanism, not a determinism mechanism.** It makes every frame identical
so that every piece of frame-timing-derived state is trivially equal, which is why nobody ever had
to ask where that state lived. Believing the docs' premise sends the next person off improving
clock fidelity instead of finding the unsynchronised state.

That heading is now "The pinned step is still required, and not for the reason it looks like", which
is the correction this section argued for, landed in #625. The distinction it preserves is the one
to keep: the pinned step is still required *today*, so the warning telling a player to relaunch with
`--fixed-dt 16` remains right. What changed is why. The old heading made a claim about the world and
was false; the warning makes a claim about this build and is true. The difference that matters is
between "the host clock is unreproducible" and "there is hidden state and one flag was hiding it" --
the second names work that can be finished, and three pieces of it have been.

The rule to adopt out of that is worth more than the specific fix:

> **When the recorder wants a setter on game-world state, the savegame is not restoring enough.**
> The answer is not an accessor pair on `CONTROL`; it is to put the variable with the function that
> computes it, and then to decide whether it belongs in the save, the digest, or both.

That rule was followed on the carry -- `Timer_PlanSimSteps` lives in `TIMER.CPP`, so the carry
belongs in `TIMER.H` beside `TimerRefHR`, which `RECORD.CPP` already reads and writes -- and it cost
no accessor and no build-graph exception. (An earlier draft of this review also credited it with
taking `DEFINES.H` fan-in from 57 to 56. That was wrong, and is one of the cases collected under
"Oracle discipline" in [BUG_HUNTING.md](../BUG_HUNTING.md): the count came
from a tree configured without `LBA2_BUILD_TESTS` and so was a translation unit short. On main
`DEFINES_FANIN` is 57 both before and after.) That is the shape
to repeat, and it scales: each such variable found is one less thing `--fixed-dt` has to mask, and
the day the list is empty, items 1 and 2 leave the ledger together.

**One known exception, so the rule is not read as covering everything -- and it has since been
closed.** The hero's animation anchor was stamped by `InitAnim` during boot, *before any tick existed
to own it*, and the load then moved the clock without moving the anchor. That was a load-path
ordering bug in its own right rather than a symptom of the missing tick, so it would have survived
the engine-tick work rather than being absorbed by it. `LoadGame` now puts the hero on the restored
clock, merged as part of #625 (`32a8db9f`). It is kept here as the worked example rather than
deleted, because the shape recurs and is worth recognising on sight: state stamped before a load,
from a clock the load then moves. The fix that survives is re-anchoring dependent state where the load restores
the clock, which is what `Timer_PlanSimSteps` already does on a game-clock rewind for the same
reason. Making the boot clock itself deterministic is the option to avoid: `TimerRefHR` at boot
feeds `Rnd_Seed` at `ChangeCube` on the fresh-start path, so it would change normal play, and the
engine-tick work would want to redo it properly afterwards.

## The digest needs a stated membership rule

The best design finding of this review, and it is not mine.

`Control_StateDigest` mixes five globals that no savegame carries: `NumObjDial`, `GameChoice`,
`GameNbChoices`, `FlagFade`, `FlagBlackPal`
([CONTROL.CPP:1754-1758](../../SOURCES/CONTROL.CPP#L1754)), where the `M` macro hashes and names in
one expression, so each is a digest member and a telemetry field at once. The same five fill the
keyframe vector at :1688-1692, which is a separate consumer and not the digest.
They survive the recorder's own reload inside the recording process and start at their
`GLOBAL.CPP` initialiser in a fresh replay, so they can mismatch at tick 0 without anything having
diverged. `dial.obj` is already documented as a false positive; it is one of five, and no rule
stops a sixth being added tomorrow. This one is read out of the source rather than measured, which
is why it survived the retractions above intact.

The shape problem: the digest is supposed to answer "did the simulation reach the same state", and
its membership was chosen as *interesting things* rather than *state a load restores*. Those are
different sets and nothing enforces the difference.

### The camera fields are a load-path asymmetry, which is heavier than a membership defect

On an exterior cube a mid-session `rec start` cannot replay clean at tick 0, and the fields that
differ belong to the rule's first category rather than to none of them. `BetaCam`
([CONTROL.CPP:1722](../../SOURCES/CONTROL.CPP#L1722)) and `VueOffsetX`, `VueOffsetY`, `VueOffsetZ`
(:1727-1729) are digest members, and the savegame writes and reads all of them unconditionally in
its camera block ([SAVEGAME.CPP:1314](../../SOURCES/SAVEGAME.CPP#L1314) and
[:1973](../../SOURCES/SAVEGAME.CPP#L1973)). The save carries them, so comparing them is correct.

What the measurement shows instead is **two load paths disagreeing about one file**. Cold-loading
the exact snapshot the recorder wrote -- one tick, no recorder in the loop -- gives `BetaCam` 2414
and offsets 11512/170/30421, which is what the replay has. The recording side, reloading those same
bytes through the recorder's own path, has 2404 and 11264/0/30208 at tick 0 *(measured elsewhere)*.
Which of the two is faithful to the file is open, and the bytes cannot settle it directly, because
saves are compressed and `CompressSave: 0` does not reach the snapshot writer.

The control is one save on one box, build and cfg: recording at the load gives `first hash mismatch
-1` and drift 0, and recording 200 ticks in gives `tick 0 state differs: VueOffsetX 11264/11512
VueOffsetY 0/170 VueOffsetZ 30208/30421`. Replaying one windowed, with audio, at the recorded
resolution reproduces it byte for byte with every mode line matching, so it is not headless, not
audio and not the platform. It needs a save with an exterior camera, which is why the shipped tests
never see it.

**A membership defect is a comparison reading something it should not. This is a load path building
a different world from the same bytes**, which is worse, and it would have stayed hidden behind the
membership reading if the save format had not been checked. `FlagBlackPal` is the field in that
recording that does belong to the class -- 0 against 1 at tick 0 in all six -- and it is the
measured instance the rule needed.

### The report has two strings for three outcomes

Six of those recordings replay their **entire session** and print `first hash mismatch 0`, which is
also what a run that fell apart at the first tick prints. That the sessions really do reproduce was
verified from the other end rather than inferred from the keyframes: replayed to the end and dumped
against the savegame the recording carries, the hero matches in every field but an animation
sub-frame -- x 2108, y 1131, z 10574, beta 4087, anim 66, life 253 -- and all 256 game variables
match, after 4852 ticks *(measured elsewhere)*. So a verdict string meaning "wrong from tick 0" is
printed over eighty seconds of perfect reproduction.

A seventh wedges the replay outright, at 99% CPU after tick 64, with the stack in `MainLoop` ->
`DoLife` -> `Dial` -> `SpeakAnimation` -> `PresentFrame`: a dialogue box presenting forever. That
one prints no verdict at all.

**Three outcomes, two strings.** A replay that diverged at the first tick and stayed wrong, a replay
that differed at tick 0 and then reproduced the session, and a replay that never finished are worth
distinguishing before any of them is acted on. The fix is the same shape as the entry-condition
argument: the outcome type needs the values the outcomes actually have.

### A contributor wrote the finding down first, without knowing it

The strongest evidence for this is not ours. A contributor sent a 90,692-tick Windows recording of a
full Any% run and, with no sight of any of the above, hand-wrote what a replayer has to arrange
first:

> "make sure to have 3 manual save files made in advance, with the save menu cursor on the 1st one
> & the inventory cursor on the Magic Ball. Then start a new game & skip the intro FMV"

That is a list of state the recording does not carry, written from experience by someone reasoning
only about their own file. The recording confirms two of the five directly: `dial.obj=2` and
`choice=490` in the recorded keyframe against `0/0` in a fresh replay *(observed elsewhere)*. Two of
the five, at tick 0, in a contributed file.

**And it extends the finding past a two-way split, because two of those three preconditions are not
process state at all.** The save-menu and inventory cursor positions are UI position, which no
savegame restores. "3 manual save files made in advance" is the *contents of the user directory*: a
recording's reproducibility depends on which save slots exist on disk, and nothing in the format or
the header expresses that.

### The rule, in the shape the evidence asks for

So the useful question is not "does the savegame restore it". For every piece of state the
simulation reads, **one of three has to be true**:

| | Where it lives | Behaviour |
|---|---|---|
| 1 | The savegame carries it | Hashed and compared. A mismatch is a divergence |
| 2 | The file carries it separately, and the replay installs it | Hashed and compared |
| 3 | It genuinely does not reach the simulation | Recorded and reported, never compared |

The defect is state in none of the three, and the five globals are in none of the three. So are the
cursor positions. So are the save slots on disk.

Category 2 is not hypothetical and does not need designing: **`bindings.digest` is already it.** The
format hashes the keyboard and gamepad tables precisely because they live outside the savegame, the
replay installs them for its duration and hands the player's own back on the way out. The shape of
the fix exists and shipped; it simply was never generalised past the one case that forced it. UI
cursor position is the obvious next member and is small. Filesystem state is the awkward one and may
be answerable only as a declared precondition rather than a carried value -- but naming it in the
header is strictly better than a contributor having to discover it and write it in an email.

`FlagFade` and `FlagBlackPal` are then category 3 or category 2 depending on whether they reach the
simulation, which is a question with an answer rather than a judgement call. That is the whole point
of having the rule: a field added to category 1 that the savegame does not carry becomes a bug in
exactly one of the two, and someone has to say which.

It also gives the refactor-oracle use what it needs, which is a category-1 set that is *complete* --
the excluded extras, projectiles and sound state named as open question 3 in the research doc become
candidates to test against a rule instead of a matter of taste.

`Control_StateDigest` is already a general primitive with three consumers (`--dump-state`, the
socket, the recorder). The membership rule is the piece it is missing, and every consumer gets it.

A small corroboration, found while this review was being written: **`CinemaMode` is mixed twice**,
once with the camera block ([CONTROL.CPP:1733](../../SOURCES/CONTROL.CPP#L1733)) and again with the
modal block ([:1753](../../SOURCES/CONTROL.CPP#L1753)). `M()` both mixes the value into the hash and
records its name, so it is hashed twice and takes two telemetry slots; names and values stay
aligned, so the output is correct, which is why it never surfaced.

Read the source and it is not carelessness, which is what makes it good evidence. `CinemaMode` is
genuinely both camera state and modal state, the comment above the second site justifies the modal
block rather than the repeat, and two reasonable groupings each claimed it. **A membership rule is
exactly what decides that**, and without one there was nothing for either author to be wrong
against.

One more thing that recording contributed, and it is a small vindication of work already done. The
contributor's own diagnosis of what breaks their replay: "what does get desynced (likely due to RNG
seed differences) is enemy knockback direction from certain types of attacks (specifically,
spacesuit laser cannon projectiles). This can stop a replay dead in its tracks." The instrument that
settles whether a divergence is a draw offset -- `rng.draws` in the digest -- was built
independently on the loose-clock branch for a different question. A contributor named the symptom
and the tooling for it already exists, unlanded.

## Telemetry, and what it costs

"Great telemetry" is the second principle, and the digest-plus-verbose design serves it well: a
divergence names the field, under the same name the digest mixes it by, with no separate name table
to drift. That part is right, and the caps discussed above are the part that is not.

Telemetry is opt-in, armed by `--record-telemetry` or `rec start verbose`. A branch that made it the
default
was built, measured and then parked, and **the reason it was parked is the best argument in this
review for the caps being a real defect rather than a nuisance.** Telemetry is 2787 bytes a tick
against roughly 101, and writing it costs 0.10 ms of CPU a tick -- figures in
[RECORDING.md](../RECORDING.md) rather than in anyone's transcript, since #621. Cite them with their
basis, **668 values a tick**, and not by the byte count alone. That is the second value of each in
two days: the digest gained fields in #625 and the bytes went 2319 to 2787 with them, re-measured
rather than scaled. So the value count is what tells a reader which regime a figure is from, and the
byte count is the part that ages. The per-tick cost is flat however long the session runs, so this
is a transfer problem rather than a perturbation one -- and transfer is what the
attach-it-to-a-bug-report use depends on.

**The size of that problem was badly understated until this week, by a bug that happened to be the
same order of magnitude as the thing it was hiding.** A real contributed session of 90,692 ticks is
96.7 MB, and 89.5 MB of that is analog blocks: 4,472,955 polls of 4,473,043, 100.0%, with `mdx` a
constant 8 on every one *(measured elsewhere)*. The block is written whenever any axis is non-zero
([RECORD.CPP:1429](../../SOURCES/RECORD.CPP#L1429)), so one stuck mouse axis puts twenty bytes on
every poll of a half-hour session. **92% of that file is a single unchanging value repeated four and
a half million times.**

Correct for it and the telemetry arithmetic changes character:

| | Size | Multiple |
|---|---|---|
| As measured, telemetry off | 96.7 MB | -- |
| As measured, telemetry on | ~307 MB | 3.17x |
| Analog fixed, telemetry off | ~7.3 MB | -- |
| Analog fixed, telemetry on | ~260 MB | **35.6x** |

(The last row moved with the digest: 2787 bytes a tick over 90,692 ticks is ~253 MB of telemetry on
top of a ~7.3 MB recording. At the previous 2319 it was 29.8x. Same conclusion, slightly worse, and
a second illustration of why the byte count wants its value count beside it.)

So **fixing the stuck axis does not shrink the telemetry problem, it stops hiding it.** A 3x
overhead is arguable; 30x on a file that would otherwise be 7 MB is not, and it is the honest number
to hold the default against. Both fixes are worth having and the order matters only for the
arithmetic: nobody should size compression work, or conclude recordings are inherently large,
against a 97 MB figure that is 89 MB of padding.

The analog fix itself needs no design decision -- a per-field presence bit, or storing the block on
change rather than on non-zero, either of which the reader learns once.

Whichever of those two branches lands second re-measures rather than scales, so there is a window
in which the merged figure is stale by one number. That is the reason to carry the basis with it.

**Against which the report names at most 24 values.** 3 ticks by 8 fields, however much the file
holds, and on main `dump_recording.py` reports counts rather than values, so nothing reads the rest
offline either. Paying that much for something unreadable past line 24 is not a trade worth making,
which is why the flip was parked.

**That reasoning is already out of date, and the replacement is stronger.** The offline reader
landed while this review was being written (#622, `4aaf7bd1`), so "unreadable" is simply false of
the file now: `dump_recording.py tele-changing` names the values that move, with no engine and no
preconditions. It remains true of the replay's own report, which is the cap. What moved the other
way is the size: the stuck-axis correction above makes the overhead ~35x rather than 3.2x, an order
of magnitude worse than when the decision was taken.

So the parking holds, on entirely different legs from the ones it was decided on -- the caps are
unchanged, the file is now readable offline, and the size argument is an order of magnitude
stronger. Anyone revisiting the default should argue against those. **"Nothing can read it" is
retired and should not be repeated.**

So the caps and the default are **one change and not two**. Separately each is weak: uncapping a
report nobody's recording carries the data for is theory, and defaulting on a payload nothing can
read is waste. Together they are the feature. That is a sharper conclusion than this review reached
on its own -- item 4 had already paired the CLI opt-out with uncapping the report, and this is the
same line one step further along.

The CLI asymmetry that pairing was aimed at is moot while telemetry is opt-in: there is nothing to
opt out of. What survives is the requirement pointing the other way, and it is worth keeping as a
**precondition on the flip** rather than as an item of its own: *if the default is ever turned on,
`--record` gets the opt-out `rec start` has in the same change, not after it.* `--record` and `rec
start` are the same operation reached from two places, and shipping a 3x cost that only one of the
two doors can decline is the version to refuse.

What shipped instead was smaller and unrelated to the default: a stale telemetry ask on two failure
paths, a leaked temp directory in the suite, and the measured figures written into
[RECORDING.md](../RECORDING.md). Ledger item 5 is unaffected by any of it.

**And one defect that was open for most of this review's life, closed while it was being written.**
`--verbose` used to do double duty through one flag: it armed the recorder's per-tick value store
*and* it turned on the scene, hero and modal-marker logging. #623 (merged, `c7617da8`) separated
them. `--record-telemetry` now arms the recorder through a new `Control_RecordTelemetry()`, and
`--verbose` does what its help text always said it did.

Kept here because the diff does not explain the shape, and a reader asking why the flag is split
this way will not find the reason in it. **The sharpest form of the defect was not that one switch
named two behaviours, but that only one of the two was documented.** `--help` said `--verbose` would
"log scene and hero state as the run proceeds" and stopped there, so someone passing the flag to
find where a headless run was hanging had no way to learn they had also turned on kilobytes a tick
in any recording that run made. A documented behaviour and an undocumented one sharing a switch,
where the undocumented one was the expensive half. That framing is what made it a fix rather than a
rename.

**And one observation survives the merge.** `Control_IsVerbose()` now has five call sites
(`INVENT.CPP:1041`, `INVENT.CPP:1458`, `MESSAGE.CPP:2099`, `PLAYACF.CPP:377`, `PERSO.CPP:2531`),
plus one direct `if (s_verbose)` read at `CONTROL.CPP:1149` that never went through the accessor at
all -- six sites, all of them logging. A split that had moved only the accessor calls would have
been correct here, but **correct by luck rather than by design**: it is correct only because the one
site that bypasses the accessor happens to be a logging site. That is still true of the code as
merged, and it is the kind of thing worth knowing before the next person assumes an accessor is the
whole surface of a flag.

Line numbers against main `def3839e`, and re-grepped rather than adjusted at every rebase. The
direct read has moved twice since the split landed -- 1123, then 1134, then 1149 -- and neither move
came from the `--verbose` work: the second was a new `--dump-state` field and a mouse-injection fix,
both unrelated. "No drift from my change" is not "no drift", which is the reason to re-grep on a
schedule rather than when something plausibly relevant merges. The general form is worth more than
the instance: **a check scoped to one change cannot certify a property of the tree, and the more
carefully it is scoped the more authoritative its wrong answer sounds.** The report that these
numbers had not moved was backed by a real line count of a real diff, which is exactly what made it
persuasive.

## Portability: what the header declares, and what it does not

`numeric.rng` and `numeric.long_double_bits` are the right idea -- traits rather than a platform, so
the file says what must match rather than when it was written -- and the glibc-compatible in-tree
generator is the choice that made Linux/Windows round trips work. That is settled and good.

The gap is that **`engine=` cannot distinguish two builds.** It carries `LBA2_VERSION_STRING`,
resolved from the checked-in `VERSION` file plus a `-dirty` marker, so two builds either side of a
gameplay-affecting fix read identical -- and so do two builds of the same source at different
optimisation levels or from different compilers. The research doc proposes a hand-bumped
`sim.compat=N` for the first half of that. I would not start there: a number a human has to
remember to bump is a thing to forget, and it says nothing about the second half.

Start with what cannot be forgotten. The mechanism is already there: the header carries
`build.flags` ([RECORD.CPP:547](../../SOURCES/RECORD.CPP#L547)) from `BUILD_FLAGS_STR`
([CMakeLists.txt:165](../../CMakeLists.txt#L165)), and it is already compared -- absent from the
ignore list, so a difference is already named at the top of a replay. It carries `SOUND_BACKEND`,
`MVIDEO_BACKEND` and `ENABLE_ASM`, and neither the compiler nor the build type.

**Add `build.compiler` and `build.build_type` as their own lines, rather than appending to
`build.flags`.** Appending is one line of CMake and it is the wrong line, for a reason the header
states about itself: "One line per key rather than a digest: `rec info` diffs the header line by
line, so a named line reports the key that differs, and a digest could only report that something
did" ([RECORD.CPP:516](../../SOURCES/RECORD.CPP#L516)). A single string holding the sound backend,
the ASM switch, the compiler and the build type is a small digest of exactly that kind -- it reports
that *something* about the build differs and leaves the reader to eyeball the string.

That matters more once the `settings.*` gate above exists, because the fatal-versus-advisory split
then has to be expressible per key. `SOUND_BACKEND` is not provenance: a null sample driver makes
`IsSamplePlaying` an unconditional no, which is a different branch, and is the whole reason
`mode.audio` is its own line. Compiler version is pure provenance. Welded into one key they cannot
be treated differently, and the choice becomes making the whole line advisory -- losing a contract
check that exists today -- or fatal, refusing every replay built by a different compiler patch
release. Separate lines cost two CMake variables and two lines in the `snprintf`, with no format or
reader change, since the header is `key=value` text and `RecFmt_ReadStr` finds any key.

Worth noticing that `build.flags` already welds three switches together, so this is a line the
header's own rule was already bending on. Appending would deepen that rather than introduce it.

**And the existing weld belongs with the gate rather than on a tidy-up list**, because it is the
same question asked of a different key. `SOUND_BACKEND` is not provenance: a null sample driver
makes `IsSamplePlaying` an unconditional no, which is the branch the ambience work turned on and the
reason `mode.audio` earns its own line. `MVIDEO_BACKEND` and `ENABLE_ASM` mostly do not change what
the simulation computes. So one line currently mixes a contract with two pieces of provenance, and
the moment `settings.*` becomes fatal, `build.flags` is the next line that cannot express the split
it needs. Not urgent; sequenced with the gate rather than after it.

**Justify the lines as provenance, not as a known fault.** The evidence that optimisation level
moves the simulation was among the figures withdrawn during this review, so the honest argument is
"a report that fires should say which two builds it was between", not "because builds diverge". The
dead figure should not be resurrected as the justification for the fix.

And leave the compiler's patch version out. Compared and warning-only, `13.2.0` against `13.3.0`
warns on every replay, and a warning that fires when nothing is wrong is the one that gets ignored
when something is -- which is the same reasoning that keeps `build.platform` out of the comparison
entirely. Vendor, major version and build type carry the diagnostic value without the noise.

`sim.compat` becomes worth adding later, when there is data on how often these lines fire falsely.

One note for whoever eventually measures a same-source divergence, since a figure claiming one was
withdrawn during this review and someone will re-take it. **Do not reach for x87 excess precision as
the explanation on x86-64.** `float` and `double` go through SSE there, so `FLT_EVAL_METHOD` is 0
and there is no excess precision to spill; `long double` spills at full 80-bit width, so
optimisation level does not change its rounding; and FMA contraction is unavailable, because the
baseline target is plain `x86-64` (`LBA2_NATIVE_ARCH` is `OFF` at
[CMakeLists.txt:76](../../CMakeLists.txt#L76)) and FMA3 needs Haswell. The `long double` and
`lrintl` work in `LIB386/3D` is doing what it was built to do. The likelier explanation would be
uninitialised reads -- state whose value depends on layout the optimiser rearranges -- and if that
is ever confirmed it is a much better outcome, because it means there is UB in the simulation and
the recorder is the best instrument this project has for locating it. Named here as the hypothesis
to test first, not as a finding.

### The one clean cross-platform number

Re-measured on merged main with `--load`, after an earlier version of it was withdrawn along with
three other figures off a broken replay arm. Same 861-tick recording, and it came back identical --
but now with clock drift 0 rather than 224 ms, and with only the mantissa width left differing
*(measured elsewhere)*:

```
[rec] 2 mode line(s) differ: engine (dirty tag), numeric.long_double_bits=53 vs 64
[rec] 861 ticks checked, first hash mismatch 0, clock drift max 0 ms
tick   0 differs: cam.beta 2231/2239  blackpal 0/1
tick 192 differs: cam.beta 3106/3109  blackpal 0/1
tick 224 differs: blackpal 0/1
```

`cam.beta` differs by 8/4096 of a turn, about 0.7 degrees, decaying to nothing by tick 224, on a
session where every other simulation field matches. That is the only clean cross-platform difference
anyone has, and it is smaller than every same-machine difference that was claimed and withdrawn --
which is an ordering argument for the ARM work on its own.

**One caveat has to travel with it.** The macOS build is `0.13.0-dev`, from before #616, so the
comparison is cross-base as well as cross-architecture. The number is what it is; attributing it
specifically to ARM arithmetic wants a macOS recording from a current build, which does not exist
yet.

**And the `blackpal 0/1` beside it is not ARM at all.** It is the digest membership problem
appearing in the same output as the finding: recorded 0, replayed 1, because the replay's `--load`
fades in. A field in none of the three categories, producing a false positive alongside a true one,
in a report a reader has to separate by hand. That is the clearest single illustration of why the
membership rule is worth having.

## Three clock facts worth keeping separate

`TimerRefHR` is non-monotonic, and each time it has been measured the scale has grown. In a
600-tick scripted session at `c00a0406`, per-tick deltas were 594 of exactly 16 ms, one +32, one
+240, and one of minus 4992 ms -- the clock going backwards about five seconds at a cube change
*(measured elsewhere)*.

**Read offline from a real 90,692-tick session, the scale is minutes and the cause is different.**
36 backward jumps, the worst of them minus 436,880 ms, landing on a handful of repeated values. The
dominant term is not scene change: **it is save loading.** The savegame carries `TimerRefHR`, so
every load reinstalls that save's baseline, and a session that loads repeatedly keeps being thrown
back to the same few readings. The arithmetic consequence is the striking part -- 90,692 ticks that
should span 24.2 minutes of game time span 3.5 *(measured elsewhere, from the file, with no engine
in the loop)*.

The third fact is that none of this breaks replay: the recording carries the refs and the replay
reproduces them, drift 0, including across cube changes. That still holds and should not be quietly
dropped now that the magnitude is larger.

**The rule that falls out of all three, and it is the usable form:** `TimerRefHR` is not a valid
rate numerator across a transition. Measured at minus 4096 ms across a single `cube` change, because
a scene change *installs* a clock rather than advancing one *(measured elsewhere)*. So any
calculation of the form "game time elapsed over this window" is reasoning from a number that can be
negative, whenever the window contains a load or a scene change --
[CONTROL.md](../CONTROL.md) now says where its monotonic advance stops being meaningful. This is the
same caution as the duration note below, stated as a predicate somebody can check rather than as an
anecdote about one session.

The three belong in different places. The jump is an engine defect with consequences of its own
([TIMING.md](../TIMING.md)); "it breaks replay" is not among them and any note saying so should be
corrected rather than carried.

The minutes-scale version is best read as a **caution for anything written next**, rather than as a
bug report against anything existing. No current consumer was found to be broken by it -- that was
checked, and a candidate was withdrawn. But the recorded clock stream sums to a seventh of the
elapsed time, so any future technique that orders events, measures a session length or answers "how
long did this take" from a recording cannot use `TimerRefHR` as its key without handling the
rewinds. That is worth knowing before such a thing is built, which is cheaper than after.

## What Doom settles, and the one place it argues against this review

Doom 3 is the model for the layer the recorder sits at, and the original Doom is the more useful
comparison for the questions this review raises, because it is a lockstep simulation whose demo
carries no state at all. Both are read in
[plan/RECORDING_RESEARCH.md](RECORDING_RESEARCH.md); three of their choices bear directly on the
sections above and one of them cuts against the framing.

**Doom's demo has no oracle, and that validates the largest decision here.** `ticcmd_t` carries a
two-byte `consistancy` field, `G_Ticker` fills it from `players[i].mo->x`, and a mismatch is
`I_Error` on the spot. But the check is guarded on `netgame && !netdemo`, so **a desynced demo plays
out silently.** That is precisely the failure mode the opening section of this review is about,
committed by the prior art, and it is why "a recording carries its own per-tick oracle" is the
design's most valuable property rather than an expensive extra. The field is worth taking; its
placement is the thing to avoid. Anyone arguing the digest is too expensive should be shown this
first.

**The modal gap is one seam, not two, and Doom prices half of it.** The research doc records that
modal time has no oracle: a session that sat in a dialogue choice held 707,624 polls against 1,472
ticks, so roughly 580,000 polls had nothing checking them. The measurements above add the other
half -- a modal is *also* unpaced. Both come from the same place: **a modal loop mints game time
without reaching the tick hook**, so neither the oracle nor the pacer follows it in. One seam, two of
this review's findings.

The pacing half is narrower than an earlier draft of this section had it, and the imprecise version
sends a reader to the wrong file. The pinned clock
mints virtual time from **three** places, not two: `Timer_FixedDtAdvance` (the tick),
`Timer_FixedDtPresent` (an extra present) and `Timer_FixedDtPump` (a non-presenting wait). The
recorder's pacer sat on the first. **Both of the others are the modal ones** *(measured elsewhere)*.

Doom's answer to the oracle half is the cheap one and it fits: two bytes a poll over the modal's own
state -- which item is highlighted, what has been chosen -- is about 1.4 MB across those 707,624
polls, against a stream already measured in megabytes. Running the full tick digest per poll would
cost more than the recording it protects; this would not. It checks only that both runs were at the
same place in the modal, which is the whole of what is missing.

The pacing half has a measurement now, and it is worse than the seam argument implied.
`FadeToBlack` (SOURCES/AMBIANCE.CPP) waits out FADE_DELAY, 200 ms of *game* time, and under a pinned
clock each iteration calls `Timer_FixedDtPump()` which hands it 16 ms of that for free. Instrumented
per step: **208 ms of clock in 26 ms of wall, eight times real speed** *(measured elsewhere)*.

**And the same run's overall rate read 1.02x**, because the paced main loop averages the spike into
nothing. That is a limitation of this review's own item 2, and it should be read as one: **an
aggregate rate assertion cannot see a modal.** The rate arm is still the right guard for what it
guards -- it catches a recorder that slows the run it observes -- but a session can pass it while a
player watches transitions race. A modal-aware check is a different instrument, and Doom's two-byte
per-poll field is the oracle half of it rather than the pacing half.

**And the place Doom argues against the observer framing.** `G_WriteDemoTiccmd` packs the command
into four bytes, rewinds the pointer, and reads it straight back over the live command, under the
comment `// make SURE it is exactly the same`. The recording session therefore runs on the same
truncated input the playback will. That is a *deliberately non-passive recorder*: it degrades the
session it observes so that a lossy encoding cannot become a divergence source. id accepted coarser
turning while recording rather than give up the guarantee.

`Record_PollHook` does not do this. The capture path writes the poll and leaves live state alone,
while `replay_inject` substitutes a reconstruction, and the two differ in one field -- pad presence
is `s_rsx != 0 || s_rsy != 0` on the replay side and `JoystickIsPresent()` on the recording side.
The comment defends it by inspecting the only consumer and concluding that a poll with no deflection
and a poll with no pad behind it are the same poll. That is true today and it is an argument about
one call site staying the only one.

This is worth resolving in Doom's direction, and it does not cost the passivity argument anything.
"Passive" here means *the recorder does not change what the simulation computes*; it has never meant
*the recorder writes the file and looks away*. Calling `replay_inject` at the tail of the capture
path makes record and replay agree by construction rather than by an argument that has to be
re-made whenever a field is added -- and since the recorded run then runs on exactly what a replay
will feed it, it makes the recording *more* faithful to what the player saw, not less. Cheap,
structural, and it removes a future divergence source rather than a present one.

**Doom's version byte is the argument against `sim.compat` starting life as a refusal.**
`G_DoPlayDemo` printed "Demo is from a different game version!" and refused on any mismatch, which
was too coarse to survive; the source ports that outlived it settled on compatibility levels, naming
the behaviour rather than the build. That is the reasoning behind reporting `build.compiler` and
`build.opt_level` rather than refusing on them, in the portability section above.

## What not to do

**Do not split the RNG for the recorder's sake.** Doom's `P_Random`/`M_Random` split is the right
shape and the analysis behind it is sound: 17 of 54 draw sites are non-simulation, and the split
would kill the `DetailLevel` divergence at tick 235 and the ambience half of the audio problem at
the source. But it was measured not to be what unblocks the loose-clock step -- a draw counter in
the digest shows both ends drawing the same number of times at and before a divergence that
reproduces five times out of five -- and that is already documented as of #619. It also invalidates
every committed baseline, corpus digest and existing recording on one commit. That makes it a
[BIT_EXACTNESS.md](../BIT_EXACTNESS.md) decision to be taken when one is being taken anyway, not a
recorder work item.

**Do not wait for the engine/render ladder -- but do not read its total cost onto its first rung
either.** The recorder's clock latch exists because the engine has no tick of its own: a present is
a tick, `BoxBlit` ([DIRTYBOX.CPP:396](../../LIB386/SVGA/DIRTYBOX.CPP#L396)) opens with a call to
`Timer_FixedDtPresent()` ([:403](../../LIB386/SVGA/DIRTYBOX.CPP#L403)), and the observer had to
build a clock because the observed system had none.
[ENGINE_RENDER_SPLIT_RESEARCH.md](ENGINE_RENDER_SPLIT_RESEARCH.md) prices the cure, and D and E in
its ladder are genuinely large and genuinely not urgent.

**Step A is not, and this review had it mis-sorted.** "A tick the engine owns; presents never
advance the clock" is priced there as *small*, *changes no behaviour*, and "mostly a consolidation
of machinery that already exists in TIMER.CPP" -- and it retires `Timer_FixedDtPresent`,
`Timer_FixedDtOverlayPresent` and the recorder's frame-clock latch, which is ledger item 3. A
refactor whose deliverable is three deletions is the cheap kind, and it ends the present-equals-tick
class outright rather than patching its next instance.

### The compensations are already the architecture

The argument for taking it is not that the split is elegant. It is that the parallel clock built to
avoid it is now larger than the thing it avoided, and the shape of its API says so.

| | |
|---|---|
| Docs circling the clock | ~1,900 lines across MOVEMENT_FRAMERATE, FIXED_DT_PLAN, FIXED_DT_RESEARCH, RENDER_INTERP_PLAN, ENGINE_RENDER_SPLIT_RESEARCH, TIMING |
| `Timer_*` fixed-step entry points | 10 |
| Call sites | 34, across 8 files, two of them in `LIB386` |
| Entry points that exist only because a present is a tick | **3 of 8** |

Those three are `Timer_FixedDtPresent` (presenting advances the clock), `Timer_FixedDtOverlayPresent`
(some presents must not), and `Timer_FixedDtPump` (a non-presenting wait needs time anyway). **The
middle one is the tell.** It is a public function whose whole job is to say *this present is not a
tick*, and it exists because the console's every-frame redraw silently doubled the game clock until
somebody replayed a session and noticed. When a timer API needs a name meaning "except this one",
the taxonomy underneath it is wrong.

So the position is not "avoid a large refactor". It is: the parallel clock was built **and** the
compensations were kept, which costs more than either end. Each compensation is individually correct
and cheap, and each is a place someone has to know a rule that is written down nowhere central --
which is the confident-wrong-answer family in
[BUG_HUNTING.md](../BUG_HUNTING.md) wearing a different costume.

One piece of step A's machinery has already arrived, unplanned: the modal pacing work funnels all
three mint sites through a single `FixedDtStep()`, which is the consolidation point step A needs. It
was paid for by a player saying transitions were too fast -- not by a refactoring budget, and not by
anyone reading the ladder.

**Read that as a prediction the ladder can be judged against rather than as an observation about one
rung: rungs that a bug walks into get built, and rungs that need their own justification sit.** It is falsifiable, which is the point of putting it in writing. If a year from
now B and C have been built deliberately, the prediction was wrong and the ladder was a schedule
after all. **Two data points so far, both from the same afternoon**: the pacing funnel arrived
because a player said transitions were too fast, and the consolidation step 10 needs arrived inside
it. A third would make this worth acting on rather than merely recording. If instead another rung arrives sideways, attached to a symptom, then
[ENGINE_RENDER_SPLIT_RESEARCH.md](ENGINE_RENDER_SPLIT_RESEARCH.md) is a **map of what will be cheap
when something walks into it**, and it should be read and maintained as one. The corollary for
anyone planning: the useful question about a rung is not "is it worth doing" but "what symptom
would arrive at it", and a rung with no plausible symptom is one to leave on the map.

**And an earlier draft of this section got the relationship wrong, in a way worth correcting rather
than quietly fixing.** It claimed the modal pacing gap and that doc's step B -- one shared pump for
the modal loops -- were the same change reached from opposite ends, and therefore that the recorder
had handed the ladder an independent customer. They are not the same change. Step B is structural:
it gives the modal loops one pump to share. The pacing fix makes the *cost* uniform at the single
place virtual time is created, and touches no loop structure at all. It fixes what a player feels;
step B fixes how the loops are built.

That distinction matters in both directions. It means the pacing work does not need the ladder, so
it can and did proceed without it. And it means **step B stays open and stays worth doing** rather
than being quietly marked as absorbed by a smaller change -- which is what the earlier framing would
have done to it.

**Do not chase the 224 ms replay clock offset**, or the other withdrawn figures below. They were
artefacts of a replay arm with no `--load`, on a base that predated #616.

## The module, and the one refactor worth doing

`RECORD.CPP` is 2848 lines against `RECORD_FORMAT.CPP`'s 208, and it carries path resolution,
header text, binding save and restore, snapshot orchestration, a three-state reload machine, clock
control, frame pacing, replay verification, telemetry, compression and console plumbing. That is
the session manager and the stream codec in one translation unit.

I would not propose splitting it on size. I would propose splitting it on the one friction that has
already cost someone a bug: **`Record_Start()` does not start recording.** On the mid-session path
it writes a snapshot, requests a reload, and returns 1; `record_begin()` runs two ticks later. So
every piece of intent is parked in a module-level static to cross that gap, and correctness depends
on every early return remembering to clear it. Two such clears had to be added when one flag's
polarity flipped, purely to stop a stale value costing the *next* recording its telemetry.

A `T_RecPending` struct, set in one place and cleared in one place, holds it. Contained, testable
without retail data, and it is the piece that stops the deferred start producing a new bug each time
the recorder gains an option.

Smaller and also real: the console splits a line on spaces and has no option parser, so `rec start
quiet` and a recording named `quiet` are the same string. Every keyword the verb gains permanently
steals a filename. Documented at `CONSOLE_CMD.CPP:386` rather than fixed. Fine to leave; worth not
adding a third keyword to.

## Sequencing

Ordered by what makes the next thing safe, not by size.

1. **Audit the replay's entry conditions**, in the shape of #616. Every precondition a replay cannot
   establish must refuse and name itself rather than proceed and report a tick. This is first
   because it is what makes every measurement anyone takes afterwards trustworthy, and the cost of
   skipping it is on record. Smaller than it sounds: `replay_report_mode` returns `void` and needs a
   return value, `mode_line_ignored` already carries the advisory half, and the fatal half is
   probably the `settings.*` prefix on its own. Splitting `SOUND_BACKEND` out of `build.flags`
   belongs here rather than later, since it is the same question asked of a key that cannot
   currently answer it.
2. ~~**The loose-clock passivity arm, as a rate assertion.**~~ **Done**, in #625: a rate arm in
   `test_record_replay.sh` carrying the "the obvious test does not work" reasoning in its comment,
   and validated by breaking it. Recorded here rather than removed, because the reasoning is the
   reusable part -- under a loose clock two runs legitimately part, so a state diff is flaky by
   construction and gets "fixed" by pinning the step, which is what it exists to rule out.
3. **The digest membership rule**, in the three-category form: the savegame carries it, the file
   carries it, or it does not reach the simulation. `bindings.digest` is the existing precedent for
   the middle category and UI cursor position is its obvious next member. Removes a class of false
   positive that makes every other result harder to read, and `--dump-state` and the socket get it
   too. A contributor has already written the missing half of this out by hand; that list is the
   backlog.
4. **Uncap the divergence report on the file.** The telemetry principle's own failure, and the
   thing currently blocking a feature: default-on telemetry was parked because 200 MB buying 24
   readable values is not a trade. The cap lift and that default are one change. If the default is
   ever flipped, `--record` gets the opt-out `rec start` has in the same change, not after it.
5. **`build.compiler` and `build.build_type` as their own header lines**, vendor and major version
   only. Two CMake variables and two `snprintf` lines; no format or reader change. Justified as
   provenance so a report says which two builds it fired between, not as a fix for a divergence
   whose measurement was withdrawn.
6. **The modal seam, both halves at once.** A modal presents without ticking, so it is unpaced *and*
   unchecked. Doom's two-byte per-poll consistency field closes the oracle half for about 1.4 MB on
   the worst session measured. The pacing half is **done**, merged as #630 (`cf561191`):
   a funnel rather than a restructure, putting all three mint sites through one `FixedDtStep()` in
   `TIMER.CPP` so a step costs a step wherever it is minted, behind a new `Timer_SetFixedDtPaced()`
   and off by default so a batch run still outruns real time. Measured after: the same fade takes
   208 ms of wall for 208 ms of clock, and step counts are byte-identical either way, which is the
   determinism argument -- pacing spends wall time and touches no arithmetic the simulation reads.
   The oracle half is unclaimed, and the fixture that would prove either is the same one -- a
   recording that opens a fade, a menu or a dialogue, which no arm currently does.
   Two things that work deliberately leaves behind, both wanting their own change: `FadeToBlack`
   pumps *and* presents each iteration, so it mints 32 ms of clock per iteration rather than 16 and
   renders half the frames it should -- correct in duration once paced, wrong in smoothness, and
   fixing it moves the clock sequence and so every digest. And a replay does not pace, which is
   right for a verification replay and wrong for a player watching one through `rec play`, who gets
   the racing that recording used to have. That one needs a policy for telling the two apart.
7. ~~**Land the loose-clock branch**, once 2 guards it.~~ **Done**, #625 (`32a8db9f`): the sub-step
   carry, the audio gate, the hero's animation anchor, the digest instrumentation and the guard.
   The pinned step is no longer masking those four; what it still masks is item 6.
8. **The `T_RecPending` struct.** Before the recorder gains its next option, not after.
9. **Store the analog block on change rather than on non-zero.** A format-layer fix worth its own
   line because of the measurement behind it: one stuck axis accounted for 92% of a real
   contributed recording. Cheap, and it should precede any work sized against how large recordings
   currently are.
10. **Step A of the engine-tick ladder, with its deliverable restated.**
    [ENGINE_RENDER_SPLIT_RESEARCH.md](ENGINE_RENDER_SPLIT_RESEARCH.md) prices step A as small and
    behaviour-neutral and names three deletions as its output. **Someone who has been inside those
    three functions says the deletions are not step A's to make**, and the reason is worth carrying
    into the item rather than discovering halfway through it. The funnel gives one place where
    virtual time is *created*; it does not give one place where the *decision* to create it is made,
    and that policy differs at each site: `Timer_FixedDtAdvance` mints and then sets
    `FixedDtSkipPresent` ([TIMER.CPP:44](../../LIB386/SYSTEM/TIMER.CPP#L44), set at
    [:151](../../LIB386/SYSTEM/TIMER.CPP#L151)) so the render that follows is free;
    `Timer_FixedDtPresent` mints unless that free present is owed or an overlay has claimed it
    ([:166](../../LIB386/SYSTEM/TIMER.CPP#L166)); `Timer_FixedDtPump` mints unconditionally, because
    a non-presenting wait owns every iteration. `Timer_FixedDtOverlayPresent` is a fourth answer.
    Collapsing four policies into one means giving the modal loops a shared pump that can express
    all of them, **which is step B** -- so the deletion half depends on step B rather than preceding
    it. What step A can take on its own is the consolidation, and item 6 has already done that for
    free. The honest deliverable is therefore: the single mint point (done) plus **a survey of
    whether the four policies can collapse at all** (not done, and the answer may be no). Filed here
    so nobody picks this up expecting three deletions.
11. **The loose-clock fade hang.** A loose-clock recording hangs at the first fade after a scene
    change, exit 124, reproducing on main independently of any branch. Measured against three
    controls, all `--headless` at cube 154: loose without `--record` exits 0, pinned with `--record`
    exits 0, loose with `--record` but no cube change exits 0, and only the fourth combination
    wedges *(measured elsewhere)*. So the recorder names it and the scene change names the site. The
    loop is `FadeToPalAndSamples` ([AMBIANCE.CPP:685](../../SOURCES/AMBIANCE.CPP#L685)), identified
    from a stack rather than a read, and the recorder's own comment predicted it
    ([RECORD.CPP:2340](../../SOURCES/RECORD.CPP#L2340)): it pumps and never polls, so a clock held
    at the last poll never lets the fade finish.

    **Two answers suggest themselves here and both are wrong, which is worth spelling out because
    each is reached by a plausible route.** The first: the quantisation is what makes a loose replay
    reproduce, so removing it inside these loops must cost reproduction -- a circular bind, and a
    hard floor under the whole loose-clock effort. The second: it costs nothing, because every one
    of these fade loops runs inside `SaveTimer()`/`RestoreTimer()`
    ([AMBIANCE.CPP:688 and :716](../../SOURCES/AMBIANCE.CPP#L688)) and `RestoreTimer` assigns
    `TimerRefHR = MemoTimerRefHR` ([TIMER.CPP:84](../../LIB386/SYSTEM/TIMER.CPP#L84)), discarding
    whatever the loop did to the clock.

    The bracket is real and the second answer stops one step short of it. Telemetry named the leak;
    no amount of reading the bracket would have. **`RestoreTimer` rewinds `TimerRefHR` and does not rewind
    `LastTime`**, which is the reading `ManageTime` banks the next delta against:
    `TimerRefHR += TimerSystemHR - LastTime` ([TIMER.CPP:348](../../LIB386/SYSTEM/TIMER.CPP#L348)).
    So a wait that ran a few milliseconds longer on one end than the other puts those milliseconds
    into the game clock *after* the rewind, and a few milliseconds is enough for
    `Timer_PlanSimSteps` to plan one extra sub-step. Reported as `sim.carry` 16 ms apart and one
    actor 32 units apart -- exactly one sub-step -- at the first tick after the scene change
    *(measured elsewhere)*.

    The lesson under it is worth more than the mechanism: **a rewind that restores one of a pair of
    coupled variables is not a rewind.** `TimerRefHR` is the clock the recorder compares, so
    restoring it looks like restoring the clock; `LastTime` is the clock's own memory of where it
    was, and nothing restores that.

    The shape that follows from the leak is to mint a **fixed** step per wait iteration -- the 16 ms
    `--fixed-dt` has been handing these same loops all along, which is why the pinned path never had
    this -- and sleep out the real time it stands for, so both ends run the same iteration count and
    leave the same reading behind. Merged as #635, with the two fixes it reported and did not
    carry following in #636 (`8b34304c`).

    **What that trades is frames, not duration, and it is the same trade the pinned path already
    pays.** A recorded fade ramps in 13 steps whatever the display could draw, where the same fade
    unrecorded ramps in as many as the frame rate allows -- 47 under the dummy driver -- while
    duration is unchanged at 1.22 s against a 1.21 s control *(measured elsewhere)*. So the honest
    statement is "the same duration, a coarser ramp", which is word for word what
    [RECORDING.md](../RECORDING.md) already says about the pinned clock's pacing. The loose path does
    not escape a cost the pinned path pays; it pays the same one.

    **And every measurement of it so far has gone through the console `cube` verb, which is the
    documented quiet path.** [plan/RECORDING_RESEARCH.md](RECORDING_RESEARCH.md) records that the
    verb is the one route leaving `FlagChgCube` zero and so skipping the `SaveTimer` lock, "which
    made it the quiet path to have built a test on".

    A risk found by reading sits nearby and **has not been shown to fire anywhere**. The pace anchor
    is taken at the first wait step and reset at the next input poll, so two wait loops sharing one
    anchor with no poll between them would leave the second finding its targets already past. Two
    candidate sites have been proposed and neither holds up. A scene transition was suggested on the
    strength of the `FlagChgCube` line above, but that line is about the `SaveTimer` lock and says
    nothing about which fades run: the fade gate in `DISKFUNC.CPP` is
    `!FlagLoadGame AND (!FlagDrawHorizon OR CubeMode + LastCubeMode != 2)`, a different predicate.
    `TM_PLAY_ACF` ([GERETRAK.CPP:227](../../SOURCES/GERETRAK.CPP#L227)) has the right shape at first
    reading -- `SaveTimer`, a fade to black, an arbitrarily long video, `RestoreTimer` -- but
    `PlayAcf` polls every frame inside its pacing loop
    ([PLAYACF.CPP:590](../../SOURCES/PLAYACF.CPP#L590)), so the anchor is reset there too.

    So the risk is **unconfirmed, unlocated, and now negatively instrumented**, which is a stronger
    statement than either candidate site would have supported. After two sites named by reading both
    dissolved on a second read, the condition itself was instrumented instead: report each wait
    step's number and whether it slept, then drive varied paths and look for a step past the ramp
    length or one that did not sleep. Neither appears. Five scene changes produced **seven** separate
    wait runs -- so two fades do happen on some transitions, and the shape is real -- and every run
    restarted at step 1, meaning a poll intervened every time *(measured elsewhere)*.

    That is the right move when candidate sites keep dissolving: **stop naming places the condition
    might hold and detect the condition.** A third dead site would have said nothing; an instrument
    that would have seen it and did not is a bound.

    **The wedge is closed and the rate is not**, and the two should not be reported together. The
    control is unambiguous, and it covers three paths rather than one *(measured elsewhere)*: eight
    recordings across a scene change wedge 8 of 8 without the fix and 0 of 8 with it; a from-boot
    loose recording hits the 90 s cap 3 of 3 without it and exits 0 in 3.5 s with it; and the demo
    reel at cube 193 goes from a 199.6 s wedge to exiting 0 in 9.1 s, against 9.3 s unrecorded. Two
    of those three were simply unrecordable before. What follows the wedge is a separate question. Of those eight, 6 reproduce -- with
    no before-figure to compare against, since before the fix there was nothing to replay.

    **The quiet-scene arms are noise against noise and must not be read as an improvement.** They
    measure 1 of 8 before and 5 of 8 after, which looks like a large gain and is not one: the hook
    does not fire at all without a scene change, traced at zero calls in both a quiet recording and
    its replay, so those are two samples of one unchanged process. [RECORDING.md](../RECORDING.md)'s
    long-standing 3 of 8 sits between them. The honest reading is that **the interval is wide at
    K=8**, which is also the right caution on the 6 of 8 above.

    So the floor under dropping the pinned step has moved rather than gone. A loose recording can
    now cross a fade; whether it reproduces afterwards is a separate question with a separate cause,
    named above: the boot `ChangeCube` seed. Measured against that fix at K=20 with three replays
    each, the disagreement column reads 4 of 20 before and 1 of 20 after, Fisher two-sided p = 0.34
    -- noise, and necessarily so, because `Timer_FixedDtPump` is called **zero** times in a quiet
    recording and zero times in its replay *(measured elsewhere)*. The fix cannot move a path it does
    not run on. That is a clean separation of two problems that had been read as one, and it is worth
    more than a partial result on either. The rate half is fixed against its own cause, merged as
    #637 (`952e77b3`) and #638 (`51fe3890`).

    **Read the scope of that with care, because the figures sound wider than they go.** Every
    measurement behind those two -- 4 of 12 to 10 of 12 reproduced, 5 of 12 to zero on the
    disagreement column, 15 of 15 to 1 of 14 under the forced hold -- is the `--load` path. From
    boot with no `--load` the seed field is never read and does not need to be: by the time cube 0
    loads, a replay is already taking its clock from the file, so both ends reach the seed on the
    same reading, at 8 of 8 seeds equal *(measured elsewhere)*. **That path reproduces 1 clean of 8,
    for reasons of its own, and nothing above touches it.** So "a loose recording reproduces" is now
    a claim about `--load` rather than about recording generally, and the from-boot rate is the open
    item those figures can be misread as having closed.

Items 2 and 7 landed in #625 while this was being written and are struck through rather than
removed, so the ordering argument stays readable. Of what is left, items 1, 3, 4, 5, 8 and 9 are
small and independent. Item 6's pacing half merged as #630 while this was being written; its oracle
half is still open and is the part that makes a modal replay mean anything. Item 10 is the one this
review was slowest to arrive at and least sure of the scope of -- read it together with 6, since 6
built the part of 10 that is already done. **Item 11 bounds a campaign rather than a feature**, and is
claimed with a fix measured and one question outstanding.

## Withdrawn during this review

Recorded so they are not re-derived. All came from replay arms that omitted `--load`, on a base
predating PR #616, where that case diverged instead of refusing.

| Figure | Status |
|---|---|
| A constant 224 ms replay clock offset | Does not reproduce at `c00a0406` |
| Audio changes the simulation: 843 of 2851 ticks | Withdrawn, unreliable |
| Optimisation level changes the simulation: 329 of 2932 ticks | Withdrawn, unreliable |
| macOS/ARM `cam.beta` differing by ~0.7 degrees | **Re-run, and survives.** Promoted to the portability section above |
| "The `TimerRefHR` jump breaks replay" | False; the replay reproduces it, drift 0 |

That fourth row is the table's own control. Four figures came off one broken arm; three did not
survive re-measurement and one did, unchanged. A withdrawal says a number was unsupported, not that
it was wrong, and re-running is the only thing that tells those apart.

Audio does still branch the simulation -- that is established independently in
[RECORDING.md](../RECORDING.md) from the ambience `IsSamplePlaying` work, and the header carries
`mode.audio` because of it. What is withdrawn is the figure, not the phenomenon.

One note on how this table was nearly undermined by the document that contains it. An early draft
argued for the build-identity lines *because* the optimisation-level figure showed builds diverging
-- four hundred lines above this table, in a review whose opening section is that a tool producing
plausible wrong answers is a design finding. The pull toward a dead but convenient number survived
knowing it was dead. That is worth leaving visible rather than quietly fixing, because it is the
same failure the recorder is being asked to protect against, and nothing about knowing the argument
makes anyone immune to it. The general collection of these, with the engine-wide instances they
turned up, is in [BUG_HUNTING.md](../BUG_HUNTING.md) under "Oracle discipline".

A second one, of a different shape, and it is about this document rather than about the recorder. A
finding offered late in the review named an existing consumer of the rewinding clock: a corpus of
state dumps ordered by sorting on `TimerRefHR`. It was specific, plausible, and did not survive
opening the directory -- those files are one snapshot per save rather than a sequence, and their
README drops `timer_ref_hr` as run-specific. It had come from a project note *about* the tree rather
than from the tree, and the note was stale. Which is the reflexive warning worth ending on: **a
review is itself a note about the tree, and this one will age into exactly the artefact that
misled.** That is the argument for the convention this document follows -- every claim about the
source carries a file and a line so it can be re-checked in seconds, every measurement carries its
basis, and the state of every branch is given rather than assumed. Treat the line numbers as "look
here", not as facts.

## Reproduce

The three things this review asks for and does not answer.

```bash
# 1. The passivity arm the fixture does not have. Same session, recorder on and off,
#    NO --fixed-dt, and compared on SPEED rather than on state: under a loose clock the
#    two runs legitimately reach different states, so a state diff is flaky by
#    construction and gets "fixed" by pinning the step, which is what it exists to rule
#    out. Assert wall-vs-game ratio, control against record, within ~10%.
ctl --load "$LBA2_TEST_SAVE" --exec-at 5 "input seq ..." --tick 600 --exit
ctl --load "$LBA2_TEST_SAVE" --exec-at 5 "input seq ..." --record probe.rec \
    --tick 600 --exit

# 2. The modal gap, which is a mechanism with no repro. The pacer runs from the tick
#    hook; the clock advances on presents. A fixture that opens a fade, a menu or a
#    dialogue while recording is what would show whether the gap costs anything real.
#    Every existing arm is a walking scene with no modal in it.

# 3. Before trusting any replay-derived measurement: prove the arm can fail. Run it
#    against a recording it should refuse, and require the refusal by name. A replay
#    that reports a tick when its preconditions were already known broken is what
#    produced every row in the withdrawn table above -- and, per the mode-line case,
#    still does on main.
```

## Related

- [BUG_HUNTING.md](../BUG_HUNTING.md) -- "Oracle discipline", where this review's general lessons
  about confident wrong answers live, with the instances from across the engine
- [RECORDING.md](../RECORDING.md) -- how to use it, and the limits as they stand
- [plan/RECORDING_RESEARCH.md](RECORDING_RESEARCH.md) -- the design work and what was measured
- [plan/ENGINE_RENDER_SPLIT_RESEARCH.md](ENGINE_RENDER_SPLIT_RESEARCH.md) -- why the observer had to
  build a clock
- [TIMING.md](../TIMING.md), [MOVEMENT_FRAMERATE.md](../MOVEMENT_FRAMERATE.md) -- the clock the
  recorder pins
- [BIT_EXACTNESS.md](../BIT_EXACTNESS.md) -- where the RNG split decision belongs
