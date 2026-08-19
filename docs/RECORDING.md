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
| `rec start <path>` | Write a snapshot, reload it, and start recording from the post-load state |
| `rec stop` | Stop and flush |
| `rec play <path>` | Replay a recording into the running engine |
| `rec info [path]` | Report the current session, or compare a file's mode lines against this run |

## `--fixed-dt` is required, not an optimisation

A session recorded on the host-sampled clock does not replay exactly. Movement integrates per
sub-step from `GetDeltaMove`, so two runs that reach a tick with the same clock reading but a
different number of steps inside it end up in different places. Pinning the step is what makes a
replay reproducible, and it was arrived at by measurement: reproducing the sampled clock,
reconstructing it, and pinning the derived game clock every tick were each built and none of them
worked.

The header records the mode, and `rec info` compares a recording's mode lines against the live run,
so a replay under a different `--fixed-dt` is reported rather than silently wrong.

## What a recording contains

| Part | Why |
|---|---|
| Polled device state, one sample per input poll | Indexed by poll rather than by tick, because a modal can spin thousands of polls inside a single tick |
| The console commands the session was driven with | Otherwise a recording cannot stand in for a harness-driven fixture |
| The keyboard and gamepad binding tables | A replay borrows them for its duration, so a recording is not tied to the cfg it was made under, and returns the player's own on the way out |
| A snapshot at each end | `<path>.lba` is where the session started, `<path>.end.lba` where it finished |
| An FNV-1a digest of simulation state, per tick | Scene, hero, camera, the other actors, the open modal, and all 336 script variables |
| A keyframe of named state, every 32 ticks | The digest says *when* a replay stopped matching; this says *what* moved |
| Every value the digest mixes, per tick, with `--verbose` | The keyframe names 23 fields. This names all of them, so a divergence in another actor or a script variable is named too |
| The settings a replay is known to turn on | They are not in the save, and a config edited in between reads as the simulation diverging |

The header is `key=value` text, so a recording is readable without a decoder. `SOURCES/RECORD_FORMAT.H`
owns the field readers and the binding-table lines; `tests/record_format` covers them.

## Verbose telemetry

A plain recording carries one 64-bit digest a tick. That is enough to detect a divergence and can
never explain one: roughly 1300 values go into it and the digest names none of them. The keyframe
narrows that to 23 named fields every 32 ticks, which covers the hero, the camera and the open
modal, and covers no other actor and none of the 336 script variables.

`--verbose` on the recording run stores every value the digest mixes, every tick, so the first
rejected tick names the fields that moved:

```
[rec] verbose telemetry: 524 values a tick
[rec] consistency failure at tick 201: recorded 2060ee23…, replayed ab660bc5…
[rec] tick 201 state differs: var.game[77] 0/42  (recorded/replayed)
```

The names come from the expression the digest mixes, so a field is hashed and reported under the
same name: `hero->Obj.LastFrame`, `obj[12].Anim`, `var.cube[7]`. A field added to the digest is
named without anyone having to name it.

The first rejected tick is reported along with the two after it, which is usually enough to tell a
value that moved once from one that is drifting. About 2 KB a tick, so it is recorded deliberately
rather than by default: roughly 4 MB for a session of a few thousand ticks.

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
config edited between recording and replay reads as the simulation diverging. Three are known to do
it, each found by changing one cfg key at a time on the replay side:

| Setting | Diverges at | Why |
|---|---|---|
| `FollowCamera` | tick 0 | The camera is in the digest |
| `FollowCamGroundClearance` | tick 2 | The camera settles at a different distance |
| `DetailLevel` 3 to 0 | tick 235 | `SetDetailLevel` turns `RainEnable` off, and rain draws from the one shared `rand()` stream |

The header carries those and the rest of the Auto camera's block, and a replay names the ones that
differ as it starts, by the key the player would edit:

```
[rec] mode differs: settings.FollowCamGroundClearance=20000 (this run: 600)
[rec] 1 mode line(s) differ; this replay may not reproduce
```

Clean on the sessions swept so far, and so deliberately not carried: `AllCameras`, the mouse
settings, and `Shadow` (`SetDetailLevel` overwrites it at MainLoop entry anyway). The boundary is
empirical rather than principled: a setting joins the block when a replay is shown to turn on it,
and a sweep that came back clean was run on one session rather than on every path.

**The mode has to match.** A recording made windowed with audio does not replay headless with the
null backend, because a live audio thread branches the simulation. `rec info` will say so.

**Recording from boot headlessly needs `--exec "skipmodals 1"`.** The opening dialogue waits for a
keypress that a headless run never sends.

## Testing

| Layer | What it covers |
|---|---|
| `tests/record_format` | Header field lookup and binding round trips. Host test, no retail data, runs in CI |
| `tests/automation/test_record_replay.sh` | A real engine records and replays, with and without a tick budget, and with verbose telemetry. Needs retail data, so it does not run in CI |

The telemetry arm changes a game variable part-way through a replay and requires the report to name
it. A reporter that printed nothing would pass a clean-replay check exactly like one that works, and
this is a diagnostic whose only job is to be believed on the day something real diverges.

## Diagnosing a divergence

A mismatch reports the tick, and the next keyframe reports the fields:

```
[rec] consistency failure at tick 101: recorded 782c3059…, replayed d69a8ed8…
[rec] tick 128 differs: hero.anim 0/1  (recorded/replayed)
```

The keyframe covers the hero, the camera and the open modal, which is most of what goes wrong. When
what moved is outside it (another actor, or a script variable), a recording made with `--verbose`
names it directly and the rest of this section is unnecessary. Without one, the tick is still the
starting point:

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
