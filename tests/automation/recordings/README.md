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

Regenerate only for a deliberate format change, and never to make a failing replay pass:
a baseline re-recorded against the build it is meant to be judging has stopped being one.

## The format file

`legacy-v10.rec` is a session recorded by a format-10 engine, kept so a build that
reads that format is asked to prove it. `tests/automation/test_record_replay.sh` replays
it.

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
