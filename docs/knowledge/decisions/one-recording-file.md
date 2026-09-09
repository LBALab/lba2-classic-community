---
type: Decision
title: A recording is one file
description: Both savegames travel inside the .rec as framed chunks, so a recording cannot be parted from the state it started from, and a torn write is refused rather than loaded.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T09:00:00Z }
constrains:
  - /formats/rec.md
sources:
  - id: recording-doc
    resource: ../../RECORDING.md
    title: Session recording (docs/RECORDING.md), "One file"
  - id: record-format-h
    resource: ../../../SOURCES/RECORD_FORMAT.H
    title: RECORD_FORMAT.H, the chunk frame
  - id: record-cpp
    resource: ../../../SOURCES/RECORD.CPP
    title: RECORD.CPP, the snapshot comment in record_begin
---

# Context

The first format named its starting snapshot as a sibling file in `setup.snapshot=`. A stream with two savegames beside it is a recording only while nothing separates them, and nothing keeps a set of files together: copy one and leave the others, and the replay reloads from somewhere the session never was. The savegames are 4 to 17 KB against a stream of megabytes, so carrying them costs nothing worth that.[^recording-doc]

# Decision

Both savegames are chunks in the stream, framed as `[op][u32 len][payload][u32 len][magic]`. Three rules about the writer do the work.[^recording-doc]

- **Nothing is written that has to be corrected later.** Each savegame exists whole before its chunk is written, so the length goes down ahead of the payload and is never revisited. A file cut inside a chunk is short of its tail; it never claims bytes that are not there.
- **The tail is the length again, then a magic word.** A half-written savegame is a valid savegame up to where it stops and the save loader would take it, so the replay would start from a state the session never reached and report a divergence with no cause. A chunk that does not close is refused by name instead.
- **The end savegame is a trailer.** It is written only while a scene is live, so a run that crashed carries no end state, and that absence is the record of the session not having finished. Everything before it still replays.

A session recorded with no cube live carries no start savegame: the header says `setup.snapshot=-` and `setup.cube=-1`, and a replay boots fresh, which is the state it began from. A snapshot written there would be a savegame whose scene is -1, which the loader does not refuse but walks off the end of the scene arrays honouring.[^record-cpp]

Measured on real truncations of one recording: cut inside the start savegame, the replay says so and checks nothing; cut mid-session, it replays the 125 ticks that reached the disk; cut inside the end savegame, it replays all 298 and stops there.[^recording-doc]

The one place a snapshot touches the filesystem is in passing. The engine's save layer works in paths at both ends, so `rec start` stages the savegame beside the recording as `<name>.staging.lba`, removed once the load has read it. Not the save folder, where the load menu would list it as a save nobody made, and named after the recording rather than shared, so two engines on one user directory cannot overwrite each other's starting state.[^recording-doc]

# Non-goals

- **Retiring the sidecar path.** A recording in an older format carries neither savegame and still names its sibling; both readers take that path too, which is why the floor `REC_VERSION_MIN` sits below `REC_VERSION`. Those are sessions somebody played, and there is no second run of them.
- **Compressing the file.** The engine's LZSS recovers 2 to 21% on a digest-heavy stream, because digests do not compress. If size ever matters, thin the oracle first.
- **Framing the poll stream.** Only the two savegames are length-prefixed. A poll record has no length of its own, so a cut inside one is simply where the stream ends, and the replay reports the ticks that reached the disk rather than a torn record. The chunk refusal does not generalise to the stream and is not meant to.

[^recording-doc]: docs/RECORDING.md, "One file".
[^record-format-h]: SOURCES/RECORD_FORMAT.H, the chunk frame comment.
[^record-cpp]: SOURCES/RECORD.CPP, the comment above the snapshot write in `record_begin`.
