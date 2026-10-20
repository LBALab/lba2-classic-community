# Committed recordings

Two kinds, kept for two different reasons: one format this build reads and does not write,
and a baseline of what the engine did before the input work started moving it.

## The input baseline

`combo-set.rec` and `combo-controls.rec` are 661 ticks each of the same scripted session
against `steam_classic_2023/Wannies fragment.LBA`, one driving the input *combinations*
the fixtures assert on and one driving each input on its own. Both replay clean, and
`test_combo_baseline.sh` requires it.

They exist because a fixture only looks where someone thought to look. The eight combo
fixtures assert the hero's animation and position; the digest behind these two covers
every actor and all 336 script variables, so a change that moves something nobody
asserted is caught here and nowhere else.

**Both, not just the combination.** Swapping `I_LEFT` and `I_RIGHT` in the turn block is
caught by the control at tick 21 and by the combination at tick 201, because a frame
holding both directions runs the same branch either way and says nothing until the
session presses one on its own. A combination without its control pins the one session
where the rule under test happens not to matter.

Regenerate only for a deliberate format change or a deliberate change to play, and never
to make a failing replay pass: a baseline re-recorded against the build it is meant to be
judging has stopped being one. A change to play earns a new file only once the divergence
is traced to that change, by showing the replay goes clean with the change backed out.
Regenerate only the file that diverged; the new one should fail with the change backed
out, at the tick the old one did.

`combo-set.rec` is format 17, recorded after the terrain slide fix of
[#745](https://github.com/LBALab/lba2-classic-community/issues/745): the session walks
into a slanted ground edge at tick 219, where Twinsen now slides instead of stopping.
It was remade again for [#801](https://github.com/LBALab/lba2-classic-community/issues/801),
which puts the FPU back in round-to-nearest after each triangle: the walk rounds as in
1997 and Twinsen's path moves from tick 111. Remade on main first, the file matched the
committed one apart from the engine line; the new one fails on main at tick 111.
`combo-controls.rec` reaches the edge once without the slide moving anything the digest
covers. It was the original format 12 file until #801 moved its walk from tick 111 as well;
it is now format 17, remade the same way (commands, ticks and keyframes unchanged, the
newer header lines added).

**Remaking one.** A recording carries the commands that drove it, so the session can be
run again exactly: list them with `scripts/dev/dump_recording.py <file>` (the `cmd @tick`
lines) and pass them back with `--record`, joining the commands of one tick with `; `,
since a run takes at most 16 `--exec-at`:

```bash
lba2cc --headless --no-autosave --user-dir "$(mktemp -d)" --fixed-dt 16 \
    --load "tests/savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA" \
    --record combo-set.rec --exec "skipmodals 1" \
    --exec-at 20 "key 80 60; key 79 60" ... --tick 660 --exit
```

Record on x86-64 Linux, as every file here was. Before committing, remake the old file
the same way with the change backed out and compare the two: the same commands, clock
readings and keyframes on every tick show the method reproduced the session.

## The menu file

`menu-esc.rec` is 200 ticks against the standard test save, ending with an ESC. It is the
only recording here whose session opens the in-game menu, and it exists because that class
had no fixture at all until the replay it broke was found by hand.

The in-game menu is a *return from* `MainLoop` (SOURCES/PERSO.CPP), and the CLI harness
calls `MainLoop` once, so a replay reaching an ESC ends the run with its stream unread.
Before the fix that exited **0** and printed `first hash mismatch -1` -- the string a caller
greps to mean the replay reproduced -- on a run that had replayed 201 of 202 polls.
`tests/automation/test_record_replay.sh` replays it and requires a non-zero status and no
`replay ended` line.

It has to be a recorded ESC rather than a driven one. The console `key esc` verb reaches
the same guard, but a replay's input arrives through `UpdateKeyboardState`, and only a file
carrying the keypress exercises the path the bug was on. That is also why the ten fixtures
that drive `esc` never caught this: every one of them presses it into a modal, which
consumes it, which is what those fixtures are for.

The arm asserts the exit path -- non-zero status, no `replay ended` line, no state dump --
rather than a verdict value, and that is deliberate. A later change that moves the
simulation makes this replay diverge before it reaches the ESC, at which point an arm
pinned to `first hash mismatch -1` would fail for a reason that has nothing to do with what
it is testing. All three assertions above still hold on a session that diverged first and
then hit the menu, because what is under test is how the run *ends*, not what it matched on
the way.

## The format files

`legacy-v10.rec` is a session recorded by a format-10 engine, kept so a build that
reads that format is asked to prove it. `tests/automation/test_record_replay.sh` replays
it.

`legacy-v13.rec` is a format-13 session that reaches the save-name screen, from the
commit before the header carried which device the player was on.
`tests/automation/test_record_input_device.sh` replays it and checks the name it saves
under. It is there for a narrower reason than its sibling: it reaches that screen by
setting the cvar from the console, so the command record in its stream is what carries
the keyboard there, and a reader that drove the flag to a default because the file did
not name one would stamp on that command and write the other name. The obvious way to
write that reader passes every same-binary arm and fails this file.

### Why a committed file rather than one the test makes

Every other arm of that fixture records a session and replays it with the same binary,
which cannot catch a change that breaks both ends together. That is not a hypothetical
class: a keyframe record grew a field, the reader took the field count from the live
build instead of from the file, and both halves agreed with each other while every
recording made before the change had become unreadable. A file captured before a change
is the only thing that notices.

So the rule is simple: **never re-record this.** Regenerating it against the current
build turns it back into the same-build check it exists to replace. If it fails, the
build has stopped reading format 10, and that is the finding.

### What it holds

| | |
|---|---|
| Format | 10, which keeps its savegames in files beside the stream |
| Session | 198 ticks, mid-session `rec start`, so `setup.reloaded=1` |
| Save | `tests/savegame/corpus/saves/steam_classic_2023/Anon1.LBA`, committed alongside |
| Files | The stream, plus `.rec.lba` and `.rec.end.lba` beside it |

The two savegames beside it are the whole point. A format-10 recording keeps its
starting state in a sibling file named in `setup.snapshot=`, and the replay finds it by
name through `snapshot_beside`. That path has no other coverage, and it is not
decorative: with the sibling the replay reports `first hash mismatch -1`, and with the
sibling moved away it diverges at tick 0. `.rec.end.lba` is never read, by that build or
this one, and is kept because a format-10 recording is three files and half an artefact
would misrepresent the format it stands for.

Recorded against a fresh settings folder, because `cam_follow` is a cvar the state
digest covers and `lib.sh` gives every fixture a `mktemp` user directory. A file
recorded with it set diverges at tick 0 for a reason that has nothing to do with the
format.

### If it fails on your machine

The header names what the session ran under, and the replay reports every mode line that
differs from the live run before it starts. `data.master=LBA2` and `numeric.rng` are the
two worth checking first: the recording was made against one retail data set, and a
simulation divergence from different game data reads the same as a reader bug until the
mode lines are compared.

    scripts/dev/dump_recording.py tests/automation/recordings/legacy-v10.rec
