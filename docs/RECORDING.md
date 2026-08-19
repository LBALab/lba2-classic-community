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
| An FNV-1a digest of simulation state, per tick | Scene, hero, camera, the other actors, and all 336 script variables |

The header is `key=value` text, so a recording is readable without a decoder. `SOURCES/RECORD_FORMAT.H`
owns the field readers and the binding-table lines; `tests/record_format` covers them.

## Limits worth knowing before you trust a result

**Give the replay more ticks than the recording holds.** Ending on `--tick` before the stream runs
out means the summary never prints, and a run that reported nothing reads exactly like a run that
passed.

**A replay needs the same `--load` the recording ran under.** For a recording made with a
boot-time `--load` that is unsurprising: `setup.reloaded` records whether the session reloaded its
own snapshot, a `--load` at boot is not that, and so nothing in the file restores the start state.

A mid-session `rec start` does reload its own snapshot, and still needs it. Measured: the same
mid-session recording replays with no mismatch when the replay is given the same `--load`, and
diverges at tick 4 without it, from both `--replay` and `rec play`. So something the reload does
not restore differs between a run that booted into the save and one that booted fresh, and a
mid-session recording is not yet self-contained either. Naming the save the session started from is
the outstanding work; until then, pass it.

**The mode has to match.** A recording made windowed with audio does not replay headless with the
null backend, because a live audio thread branches the simulation. `rec info` will say so.

**Recording from boot headlessly needs `--exec "skipmodals 1"`.** The opening dialogue waits for a
keypress that a headless run never sends.

## Testing

| Layer | What it covers |
|---|---|
| `tests/record_format` | Header field lookup and binding round trips. Host test, no retail data, runs in CI |
| `tests/automation/test_record_replay.sh` | A real engine records and replays, with and without a tick budget. Needs retail data, so it does not run in CI |

## Diagnosing a divergence

The state digest names a tick, not a field. What has worked:

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
