# Committed recordings

Recordings that a build cannot make for itself, kept so the reader can be held to them.

| File | Why it is here |
|---|---|
| `legacy-v10.rec` | Written by a build from before the keyframe carried a length. Version 10, 23 keyframe fields, 200 ticks of the Wannies exterior with no input. The only way to check that this build still reads a version it can no longer write |

A recording made by the current build proves the round trip and nothing about the past:
record and replay move together, so a change that breaks both at once passes. That is how
the keyframe grew a field without a version bump, which left every earlier recording
reading four bytes long at its first keyframe. What that cost is in
[docs/RECORDING.md](../../../docs/RECORDING.md); what stops it recurring is the length
prefix, and this file.

Replayed by `test_record_replay.sh` against
`tests/savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA`. It carries no
snapshot of its own and does not need one: it did not reload, so the `--load` supplies the
start state. Do not re-record it. A file the current build wrote is not a legacy file.
