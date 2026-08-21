# The state digest: a membership rule, and what it costs

`Control_StateDigest` hashes a set of fields to answer "did the simulation reach the same
state". Membership in that set was chosen as *interesting things* rather than *state a replay
can be made to start from*, and nothing enforced the difference. This document states the rule,
prices it, and reports a prototype that was built and measured rather than argued.

Comes out of the digest-membership finding in the recorder architecture review (PR #628), which
named the defect class and proposed a three-way rule. Two parts of that proposal are wrong and
this document says where, with the measurements that show it.

Written against `5678c7ca`. Every line number below was read with `grep -n` and re-checked
after the diff this document describes.

## The short answer

Four classes, declared per field where the field is hashed. The five globals the review found
are **class 2, not class 3**: they reach the simulation, so the file must carry them and the
replay must install them. Dropping them from the hash removes the false alarm by removing the
ability to see a real divergence, and it cannot help a single recording that already exists.

Grouped per-tick hashes are not affordable. Measured, a plain tick costs 19.8 bytes; five
64-bit group hashes are 40, which is +202%.

Neither change moves `CONTROL_DIGEST_VERSION`, and that is the substance of the versioning
answer rather than a detail of it.

## The classes

For every field the digest touches, one of these is true, and the class says which.

| | Class | What it means | Behaviour |
|---|---|---|---|
| 1 | `DIGEST_SAVED` | the replay's **load restores it** | compared; a mismatch is a divergence |
| 2 | `DIGEST_CARRIED` | the load does not restore it, so the **file carries it and the replay installs it** | compared |
| 3 | `DIGEST_LOOSE` | **nothing establishes it** in the replay | collected and named, never compared |
| 4 | `DIGEST_PROBE` | **not state**: a measurement of how the run got here | collected and named, never compared |

A field in none of these is the defect: it is compared against a value no replay can establish,
so it reports a divergence with nothing having diverged.

The class is an argument to the mixing macro rather than a comment beside it. That is the whole
of the enforcement: there is no way to add a field without answering the question.

### Class 1 is "the load restores it", not "the savegame carries it"

The review's wording was "the savegame carries it". That wording makes four correct fields read
as defects. `Island` and `CubeMode` are never written to a save; they are read back out of the
cube's own data file when the load reaches `ChangeCube`
([DISKFUNC.CPP:77](../../SOURCES/DISKFUNC.CPP#L77) and
[:88](../../SOURCES/DISKFUNC.CPP#L88)). `NbObjets` and `NbZones` arrive the same way. They are
restored without being bytes in the file, and they are fine.

The same correction rescues the hero and the actors. A `grep -c` over `SAVEGAME.CPP` for
`Obj.Alpha`, `Obj.Beta`, `Obj.Gamma`, `Obj.Body`, `Obj.Anim` and `LastFrame` returns **zero for
every one of them**, and all six are carried: the save writes the whole `T_OBJ_3D` as a struct
([SAVEGAME.CPP:1178](../../SOURCES/SAVEGAME.CPP#L1178),
[:1182](../../SOURCES/SAVEGAME.CPP#L1182)) and reads it back the same way
([:1471](../../SOURCES/SAVEGAME.CPP#L1471), [:1473](../../SOURCES/SAVEGAME.CPP#L1473)).

**A name-grep over the save is not the oracle for this question.** It clears six fields for the
wrong reason and condemns four that are innocent.

### Class 4 is not the fourth class the review proposed

The review proposed a fourth class of *asserted about the environment* -- save slots on disk, UI
cursor position -- checked at arm time, with a replay that cannot establish them refusing by
name. That is right, and it is not a digest class, because none of it is in the digest. It
belongs to the header and the arm-time check, which is where `mode differs` already lives.

The fourth class the *digest* needs is a different one. `rng.draws` is collected and printed and
is not state at all, and until now it had no name for that -- which is why it kept appearing in
divergence reports as though it were part of the verdict.

## The five globals are class 2, and this is where the review is wrong

`NumObjDial`, `GameChoice`, `GameNbChoices`, `FlagFade`, `FlagBlackPal`. None of the five appears
anywhere in `SAVEGAME.CPP` -- `grep -c` returns 0 for all five names. The review put them in
class 3. They are class 2, because each reaches the simulation:

- `GameChoice` is a Life-script conditional. [GERELIFE.CPP:238](../../SOURCES/GERELIFE.CPP#L238)
  is `Value = GameChoice;` under `case LF_CHOICE`, so a stale one takes a different script
  branch, and it does so in silence.
- `FlagFade` is read by the track interpreter at
  [GERETRAK.CPP:229](../../SOURCES/GERETRAK.CPP#L229), where it decides whether
  `FadeToBlackAndSamples` runs at all.
- `FlagBlackPal` gates the same at [AMBIANCE.CPP:541](../../SOURCES/AMBIANCE.CPP#L541), where
  `FadeToBlack` returns early on it.

A fade that runs when it should not is not only a picture. Both fade loops pump
`Timer_FixedDtPump`, so under a pinned step the flag decides how many steps get minted, and the
clock is what seeds the RNG at the next `ChangeCube`.

Moving them to class 3 therefore buys a clean tick 0 by giving up the oracle. **The rule found
the defect and then refused the repair that was proposed with it**, which is the difference
between a rule and a preference: a rule is only worth having if it can say no to the person
holding it.

## What was measured

Nine contributed recordings, replayed on a Linux build of `5678c7ca`, headless, `--fixed-dt 16`,
each with `--load` and its own copy in a fresh user directory.

The nine are not in one place, and two sweeps of "the contributed recordings" have covered
different sets because of it. Seven are in a downloads folder (one Windows, six macOS) and two
are Linux recordings inside another worktree. `~/lba2-recordings` holds eighteen
`.rec` files and **none of them is one of the nine**: they are local development captures,
formats 2 through 12.

### Two arms, because the config is a confounder

**Arm A**, replaying under the reader's own config: **9 of 9 mismatch at tick 0.**

**Arm B**, seeding the replay's `lba2.cfg` from the recording's own `settings.` header lines:
**7 of 9.** The two that clear are the two Linux recordings, which then first fail at tick 1 on
`obj[4].Anim 279/277`.

Arm B is not simply the better arm. Seeding the settings turns `FollowCamera` on, which is the
condition for the camera fault filed as #642, so three files gain `cam.dist` and `VueOffset`
differences in arm B that they did not show in arm A. The two arms answer different questions
and both are reported.

### The review's "all nine" is 7 of 9

`FlagBlackPal` differs at tick 0 in **seven** of the nine, not all nine. This was predicted
offline before any replay was run -- the recorded keyframes show `blackpal=0` in seven files and
`blackpal=1` in two -- and then confirmed by replaying. The two exceptions are the two Linux
recordings, which already carry `blackpal=1`.

### Split by what each file can actually show

Four of the nine carry no telemetry, so on those the report can only compare the 24 keyframe
fields against roughly 1300 hashed. A rate averaged over both kinds is two different
measurements added together.

| Recording | Telemetry | Arm B: compared fields differing at tick 0 | Class 2 only? |
|---|---|---|---|
| epmager (windows) | none | `dial.obj` `choice` `blackpal` | yes -- **inferred from 24 fields** |
| 161650 (macos) | none | `cam.beta` `cam.dist` `blackpal` | no -- camera (#642) |
| 175522 (macos) | none | `cam.beta` `cam.dist` `blackpal` | no -- camera (#642) |
| 181204 (macos) | none | `dial.obj` `blackpal` | yes -- **inferred from 24 fields** |
| 184751 (macos) | 3215 ticks | `NumObjDial` `FlagBlackPal` | yes -- **measured over all ~1300** |
| 142050 (macos) | 2918 ticks | `FlagBlackPal` | yes -- **measured over all ~1300** |
| 142552 (macos) | 1237 ticks | `VueDistance` `VueOffsetX/Y/Z` `FlagBlackPal` | no -- camera (#642) |
| 193719 (linux) | 273 ticks | none at tick 0 | clean at tick 0 already |
| 194558 (linux) | 12744 ticks | none at tick 0 | clean at tick 0 already |

The class accounts for the whole tick-0 mismatch in **four** of the seven, and in two of those
four the claim rests on 24 named fields rather than on all of them. The other three are the
camera, which is a different fault.

### The ceiling, and why it is only a ceiling

The arm the review proposed -- the five dropped from the hash behind
`CONTROL_DIGEST_VERSION 3` -- was built and measured. It works on the mechanism: a fixture that
mismatched at tick 0 replays with `first hash mismatch -1` once the fields are not compared.

It buys nothing for any recording that exists. A recorded hash was computed with the five mixed
in, so the replay has to mix them in too or every tick differs. The version gate is therefore
mandatory, and the fix is not retroactive: re-running all nine under that build produced output
**identical** to the baseline, line for line. Whatever this direction buys, it buys it only for
recordings not yet made -- and by then the class-2 repair is available and is strictly better. It is reported as a ceiling on what membership alone can buy, not as a candidate fix.

That arm is kept as a patch rather than a commit, because a change that must not ship should not
sit in the history of a branch that should.

### The class-2 prototype, which is the version that could ship

The recording writes the five as `state.` header lines and the replay installs them for its own
duration, on the borrow-and-return `bindings.digest` already uses. The five stay compared.

Built and measured:

- the fixture that mismatched at tick 0 now replays clean, **with all five still in the hash**
- lying about one carried value in the header (`state.dial_obj=0` rewritten to `=7`, same byte
  length so nothing else moves) is caught at tick 0 and names `NumObjDial 0/7` -- which proves
  the install reaches the global *and* that the field is still compared
- deleting one of the five lines makes the install refuse the whole set and say so by name, and
  the original tick-0 difference returns unchanged
- the nine existing recordings replay **identically** to the baseline, and are told once, by
  name, that they predate the lines
- `tests/automation/test_record_replay.sh` passes, including its deliberate-divergence arm

The install is all or nothing. A header carrying three of the five would leave the other two at
the replaying run's values, which is a state neither run was ever in and which no report names.

Cost: **78 bytes, once per file**, against a header of about 1.6 KB and a savegame of about 10 KB.

## Two more members, found by applying the rule

**`FollowCamera` is a cfg setting inside the hash.**
[CONTROL.CPP:1843](../../SOURCES/CONTROL.CPP#L1843) mixes it;
[CONFIG_FILE.CPP:101](../../SOURCES/CONFIG_FILE.CPP#L101) binds it to the `FollowCamera` key. The
recording carries the value as a `settings.` line and the replay reports the difference without
installing it, so the digest compares a value the replay was never given. It is the whole of the
tick-0 mismatch in two of the nine under arm A. Checked one at a time against all 28 distinct
scalar globals in the digest: **it is the only config-bound one**, which makes the repair a
single field rather than a settings-installation project.

**The digest version setter accepted any number.** `Control_SetDigestVersion` took `v` and stored
it, so a file declaring `numeric.digest=99` from a future build would select a set this build
does not have, hash today's fields under tomorrow's number, and mismatch on every tick -- which
reads as a session that came apart at tick 0. This is the cheapest thing in the whole area and
depends on none of the rest.

## The versioning decision

**Refuse politely on an unknown version; support every known one below the current.**

Support downwards because it is nearly free: fields are appended and never interleaved, so
selecting an older version reproduces its sequence exactly rather than approximating it, and the
recordings that exist are sessions somebody played once and cannot play again. Refuse upwards
because no honest approximation is available -- the fields are not in the binary. The failure
without the refusal is not an error message; it is a wrong answer that looks like a real
divergence, which is the expensive kind.

Implemented: the setter returns 0 for a version above `CONTROL_DIGEST_VERSION` and changes
nothing, and the replay closes the file and names the version it does not know. Validated by
rewriting `numeric.digest=2` to `=9` in a real recording, same byte length:

```
[rec] ... hashes digest set 9, this build knows up to 3; nothing to compare against
```

### Neither prototype moves `CONTROL_DIGEST_VERSION`, and that is the point

A membership change is a digest-version change -- that part of the review holds. But *repairing* a
class-3 field into class 2 is not a membership change, because the field was already being
hashed and still is. What changes is the file, not the set. Carrying and installing the five
therefore belongs to the format, not to the digest version.

It does not move `REC_VERSION` either, by that constant's own stated rule: a change that alters
what an existing record *means* moves it, and a change that only *adds* one does not. These lines
only add, and their absence is handled explicitly -- all-or-nothing install, said once by name --
so an old file is not ambiguous, it is a file that predates them.

The cost of not bumping falls on an older build reading a newer file: it does not know the five
keys and reports them as five spurious `mode differs` lines. That is noise in one direction
against refusing the file outright in the other.

## Grouped hashes: priced, and not worth it as proposed

The problem is real. A mismatch says "tick 61" and nothing else, which is why the telemetry
side-channel exists at all. The proposed fix was per-group hashes at about 40 bytes a tick
against a ~101-byte baseline.

The baseline is wrong. 101 bytes is the size of a *keyframe record*, and a keyframe is one tick
in 32, so amortised it is about 3.2 bytes a tick. Measured by recording the same session at 200
and at 800 ticks and taking the slope, so the fixed header and the two savegames cancel:

| | bytes a tick |
|---|---|
| plain recording | **19.8** |
| with `--record-telemetry` | **2698.8** |

Five 64-bit group hashes are 40 bytes a tick: **+202% on a plain recording**, tripling it.

CPU is not the constraint. Five repeats each, user time rather than wall, 2000 ticks: plain
4.4090 ms a tick, recording 4.4900 ms a tick. So the digest, the keyframe and the file write
together cost at most **0.081 ms a tick**, about 1.8%.

What survives the pricing, if the need is felt again:

- **Group hashes at keyframe cadence**, one tick in 32: 1.25 bytes a tick, +6%. But that is the
  keyframe's own cadence, and the keyframe already names 24 fields there, which tells a reader
  more than which of five groups moved.
- **Group hashes emitted only from the first mismatching tick.** Costs nothing until something
  goes wrong. The replay knows its own values at that tick; what it lacks is the recorded ones,
  and by then the stream is past.

The recommendation is neither. The gap is not that the keyframe is coarse in *groups* -- it is
that its 24 fields miss the other actors and all 336 script variables entirely. Spending the same
bytes on widening what the keyframe names would buy a reader with a divergence in hand more than
telling them which of five groups to look in.

## Telemetry width is a range, not a number

Any per-tick figure quoted about telemetry has to carry one, because the recorder writes the
value count per record and that count tracks the actor population of the cube.

Measured over the **five of the nine that carry telemetry at all** -- 184751, 142050, 142552,
193719 and 194558; the other four, including the 90,692-tick Windows run, carry none:

**434 to 956 values a tick**, with up to **seven distinct widths inside a single file** (194558:
452, 461, 515, 542, 578, 614, 956).

A parallel sweep by another session found 434 to 733 over seven files out of fifteen. Neither
range is wrong and both are partial: the two sweeps covered different sets of files. The
disagreement is the finding, not something to reconcile.

## What the classes changed about the report, and a defect they caught

Moving a field out of the comparison is not enough on its own. The replay's divergence report
compares recorded against live for every collected value and did not know the difference, so
`FlagBlackPal 0/1` kept appearing under `state differs` after the digest had stopped comparing
it -- the false alarm moved out of the hash and into the prose. `rng.draws` had been doing exactly
this since it was added.

The report now asks the class and prints two lists:

```
[rec] tick 199 state differs: var.game[77] 0/42  (recorded/replayed)
[rec] tick 199 also differs in 2 field(s) the digest does not compare: FlagBlackPal 0/1 rng.draws 3426/3114
```

The keyframe reporter needed the same split, and giving it one caught a defect introduced by this
work: with the digest's five moved to class 2 and the keyframe table's five left at class 3, the
two tables disagreed about the same five fields, and the report said the digest did not compare
`dial.obj` while the digest was comparing it. **Two tables naming the same fields will drift.**
That is the argument for one table rather than a declaration at each site, and the fastest
possible demonstration of it, since the drift was introduced and caught within the hour.

The eight-field display cap now reports its own overflow, so a report showing eight fields no
longer reads as "eight fields moved".

## What each job selects

Three consumers, three jobs. Designing for replay verification is right, and for a narrower
reason than "it is the hardest": it is the only one where being wrong is **silent**. A
`--dump-state` that shows a field it should not is read by a person who can see it. A replay that
compares a field it should not prints a divergence indistinguishable from a real one.

| Job | Classes | Timing-derived state |
|---|---|---|
| Replay verification | 1 and 2 | in -- two runs of one file must agree on `sim.carry` and the animation anchors |
| Refactor oracle across builds | 1 and 2 | out -- the timing-invariant subset, which `tests/savegame/corpus/baselines/` splits by hand today |
| Live inspection | all four | all shown, none compared |

The selection belongs in one place, and the class column is what makes it expressible.

## What is proved and what is argument

**Proved by measurement:**

- 9 of 9 mismatch at tick 0 under arm A; 7 of 9 under arm B, N=9
- `FlagBlackPal` differs at tick 0 in 7 of 9, not 9 of 9 -- predicted offline, then confirmed
- the five account for the whole tick-0 mismatch in 4 of the 7: two measured across all fields,
  two inferred from 24 keyframe fields
- the class-2 prototype clears the fixture while still comparing all five, and is caught both
  when the carried value is a lie and when the set is incomplete
- the nine existing recordings replay identically under both prototypes
- 19.8 bytes a tick plain, 2698.8 with telemetry; five group hashes would be +202%
- at most 0.081 ms a tick of CPU for digest, keyframe and write together
- 434 to 956 telemetry values a tick, N=5
- the version refusal fires and names the version, validated by breaking it on purpose

**Argument, not measurement:**

- that `FlagFade` and `FlagBlackPal` reach the simulation *through the clock*. The read sites are
  cited and the pump is inside both loops, but no fixture has been built in which the number of
  minted steps changes the outcome. The `GameChoice` case is read straight off `LF_CHOICE` and
  needs no fixture.
- that the remaining three tick-0 mismatches are entirely #642. They differ on camera fields and
  #642 is about camera fields; nobody has fixed #642 and re-measured.
- that widening the keyframe beats grouping. The costs are measured; which one a reader with a
  divergence in hand would rather have is not.

**Not known:**

- what the two inferred rows would show if those files carried telemetry. Both are contributed
  sessions from other platforms and cannot be re-recorded here.
- whether any *other* digest field is in none of the four classes. The 28 distinct scalars were
  checked one at a time; the per-actor block and the two variable arrays were checked as blocks,
  and a block is a weaker check than a field.

## Reproduce

```bash
# The nine, replayed under the reader's own config (arm A).
lba2cc --headless --user-dir "$UD" --fixed-dt 16 \
       --load "$LBA2_TEST_SAVE" --replay target.rec --tick 64 --exit

# Arm B: seed the replay's cfg from the recording's own settings lines first.
head -c 4096 target.rec | tr -d '\r' | grep -a '^settings\.' \
  | sed 's/^settings\.\([A-Za-z0-9_]*\)=\(.*\)$/\1: \2/' > "$UD/lba2.cfg"

# The fixture the shipped tests do not have. Change cube first, so the fade-in clears
# FlagBlackPal, and only then start recording: a recording started at boot has
# blackpal=1 on both sides and replays clean, which is why this never showed up.
lba2cc --headless --user-dir "$UD" --fixed-dt 16 --load "$LBA2_TEST_SAVE" \
       --exec-at 20 "cube 4" --exec-at 60 "rec start verbose" --tick 400 --exit

# Bytes a tick, by slope, so the header and the two savegames cancel: record the same
# session at 200 and at 800 ticks, plain and with --record-telemetry, and difference them.

# CPU: user time over five repeats, not wall. Wall is dominated by the write and by
# whatever else is running on the box.
/usr/bin/time -f "%e %U" lba2cc --headless --fixed-dt 16 --load "$SAVE" --tick 2000 --exit
```

## Related

- [../RECORDING.md](../RECORDING.md) -- the format and how to use it
- [RECORDING_RESEARCH.md](RECORDING_RESEARCH.md) -- where the digest came from
- [../BUG_HUNTING.md](../BUG_HUNTING.md) -- oracle discipline, which is what the class column is for
