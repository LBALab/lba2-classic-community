---
type: Format
title: Session recording file (.rec)
description: One self-framing file per session, format 14 with a read floor of 9, a text header, both savegames as framed chunks, and a binary record stream indexed by input poll.
status: draft
equivalence: tested
as_of: d9cf303d
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T16:00:00Z }
owner: /subsystems/recording.md
constrains:
  - /subsystems/recording.md
verified_against:
  - /references/record-tests.md
relates_to:
  - /decisions/one-recording-file.md
  - /decisions/digest-membership.md
  - /formats/lba-save.md
  - /decisions/the-verdict-withholds-success.md
sources:
  - id: record-cpp
    resource: ../../../SOURCES/RECORD.CPP
    title: RECORD.CPP, the writer and the reader
  - id: record-format-h
    resource: ../../../SOURCES/RECORD_FORMAT.H
    title: RECORD_FORMAT.H, the chunk frame
  - id: dump
    resource: ../../../scripts/dev/dump_recording.py
    title: The reader that needs no engine
  - id: recording-doc
    resource: ../../RECORDING.md
    title: Session recording (docs/RECORDING.md)
  - id: replay-test
    resource: ../../../tests/automation/test_record_replay.sh
    title: The record-and-replay fixture, the movement arm's comment
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, the digest's emission order in Control_StateDigest
  - id: coverage-pr
    resource: https://github.com/LBALab/lba2-classic-community/pull/659
    title: The replay coverage gate, whose description leaves the mode predicate out on purpose
---

The layout and the versioning rule are structural. The header line table and the current version numbers are read from the commit in `as_of`.

# Evidence

`tested`: the host test covers the header readers, the binding-table round trip and the chunk frame, and the automation fixture records and replays through a real engine, with and without a tick budget, across a video, and against a recording an older engine wrote. There is no disassembly to check against; the format is new construction. The second reader, `scripts/dev/dump_recording.py`, has its own copy of the record table and no compiler to notice when it is stale.[^record-cpp]

# Layout

Little-endian throughout. Four parts, in order.

| Part | Shape |
|---|---|
| Header | `LBA2REC 14`, then `key=value` lines, then a blank line. Text, so `rec info` can diff it against a live run without a parser. At most 4096 bytes. |
| Start savegame | A chunk with op `0x70`: the state the session started from, written once between the header and the first record, when the session began inside a game. Its payload is a compressed file of [the .lba Format](/formats/lba-save.md), written by the engine's own save path under the player name `rec`. A session recorded with no cube live writes none and declares `setup.snapshot=-`, because a fresh boot is the state it began from. |
| Record stream | One flags byte per record, then a payload whose length the reader must already know. Flushed every tick. |
| End savegame | A chunk with op `0x71`, written only when the session stopped while a scene was live. Its absence is the record of a session that did not finish, which is what a crash repro wants. |

## Chunk frame

`[op][u32 len][len bytes][u32 len][u32 magic]`. The three writer rules that make a torn chunk detectable are owned by [one recording file](/decisions/one-recording-file.md), and the frame's own reasoning by the comment in RECORD_FORMAT.H. A length above 16 MiB is refused before it reaches `malloc`.[^record-format-h]

## Record stream

One flags byte per record, then a payload. The file comment at the top of RECORD.CPP owns the layout of every record, bit by bit, and `scripts/dev/dump_recording.py` is its second reader; neither is repeated here.[^record-cpp] What the layout guarantees, and where it bites:

- A poll where nothing changed is one byte, and the low flag bits say which of the key table, `Key`, the analog block and the clock delta follow. The analog block is written when it differs from the last block written, not from zero, so the poll that releases a held button or stick is recorded.
- A record's length is known only to a reader that knows its type. The dispatch is therefore a whitelist, and a type the reader does not know loses the stream until the next sync marker, one every 64 polls.
- A console command is stamped with the tick already recorded, so stream order is execution order. The keyframe carries its count since version 12, and versions 9 to 11 wrote exactly 23 fields. The device record carries no count and cannot grow.
- A new record type costs four sites, and missing one fails a different way: the whitelist and the step-over chain in `replay_poll`, and the table and the decoder in `dump_recording.py`. A type registered in the chain alone works only when it happens to follow a sync marker, so a fixture for a new type wants records on both sides of a marker. Nothing in the stream may be sized from the live build.

## Header lines

Grouped by prefix. A compared line is diffed against the live run when a replay starts and reported as `mode differs`; an installed line is payload the replay takes and the mode diff skips, because the digest is what compares its effect.[^record-cpp]

| Prefix | Lines | Treatment |
|---|---|---|
| none | `engine` | compared: the version string of the build that wrote the file |
| `mode.` | `headless`, `audio`, `fixed_dt`, `resolution`, `language` | compared; `verbose` installed, since telemetry does not reach the simulation; `step_armed_tick` carried and not compared, since a pair that armed on different ticks was measured to reproduce and the line is for the reader |
| `build.` | `flags` | compared; `platform` recorded and skipped, because cross-platform replay is the point |
| `numeric.` | `rng`, `long_double_bits`, `digest` | compared: the arithmetic and the digest set a replay has to agree with |
| `data.` | `master` | compared: the data lineage |
| `input.` | `keyboard` | installed: the device the player was last on |
| `setup.` | `cube` | compared: the scene the session began in, and below zero the reader's cue that it began at a fresh boot |
| `setup.` | `snapshot`, `reloaded`, `reload_clock` | installed: how the session started, acted on rather than matched |
| `clock.` | `timer_ref_hr`, `sim_carry`, `rng_seed` | installed: the baseline, the sub-step carry, the boot seed |
| `bindings.` | `keyboard`, `gamepad` | installed; `digest` compared |
| `state.` | `dial_obj`, `choice`, `choices`, `fade`, `blackpal` | installed: the five globals no savegame carries, all or nothing |
| `settings.` | `FollowCamera`, the follow-camera, mouse-camera and gamepad-camera tuning keys, and `DetailLevel`, each named by its lba2.cfg key | compared, and not installed: a replay reports the difference and runs on its own config, so an operator whose auto-camera setting differs from the recording's sees a divergence at tick 0 that is a config mismatch. The list is empirical, one cfg key changed at a time on the replay side of a recorded session, and a key is there because its mismatch was measured to diverge: `FollowCamera` at tick 0, `FollowCamGroundClearance` at tick 2, `DetailLevel` at tick 235 through the rain's draws on the shared random stream. A mismatch can also suppress evidence: matching `FollowCamera` to a recording's value revealed a camera-distance difference at tick 0 that the mismatched run had never computed. That a mismatch can turn a run that would have diverged into a clean verdict is plausible and not demonstrated. Written by the harness, not the recorder. The architecture call on the table is to move these to installed beside `bindings.` and `state.`; read this row at `as_of`.[^control-cpp] |

The comparison runs both ways. A recorded line the live run contradicts is `mode differs`, and is evidence about reproduction. A trait the live build emits that the recording lacks is `mode undeclared`, with a count line saying those dimensions were not checked; it says only that the file predates the trait, and predicts nothing. The two prefixes are kept apart so the first cannot cry wolf on a file that reproduces. Both are printed with the boot banner and neither reaches the verdict: the `replay ended` line names the ticks checked, the first hash mismatch and the drift figures, and carries no count of mode differences, so a replay that diverged for a reason the header announced reads the same as one that diverged for none. That is a recorded choice rather than an oversight: the coverage gate the verdict does carry, two limbs that withhold the success string when the run did not get through the file, named this as a sibling defect on a different predicate, whether the run was comparable to the one recorded rather than whether it covered it, and left it out to get its own calibration.[^record-cpp][^coverage-pr]

# Versioning

Three cases, kept apart on purpose.[^record-cpp]

| Change | Moves | Rule |
|---|---|---|
| alters what an existing record means | `REC_VERSION_MIN` up to `REC_VERSION` | a reader that cannot tell the versions apart must refuse the file |
| adds a record type | `REC_VERSION` only | an older recording reads as it always did; an older build refuses the newer file outright, because the dispatch is a whitelist and cannot step over an unknown type |
| changes what the engine computes | neither | the format still means what it meant; the `numeric.` lines report the difference per recording |

Current values: `REC_VERSION` 14, `REC_VERSION_MIN` 9, `CONTROL_DIGEST_VERSION` 2. The last is the set of fields hashed, declared as `numeric.digest`; an unknown higher value is refused and every known lower one is reproduced, because fields are appended and never interleaved. Version 12 changed a meaning and left the floor at 9 on purpose: only a mouse-camera session reads differently, and no such session had ever replayed clean.

# What it does not tell you

| Field or property | What it does not say |
|---|---|
| `engine=` | Which build wrote the file. It carries the version string, which moves at a release and gains `-dirty` on an unclean tree, so two recordings four days apart across a merged fix both read the same. There is no build identity in the header. |
| `mode.fixed_dt=` | When the step was pinned. It records that the step was pinned, and at what value. Without the flag on the command line, `rec start` arms the step after the load while a `--replay` arms from the header before it, and both declare `mode.fixed_dt=16`.[^replay-test] |
| `mode.step_armed_tick=` | That the two runs armed on the same tick. It says when this run armed, written as -1 by a run that never armed a step, and it is carried rather than compared, so a recording that armed on tick 20 replays against a run that armed on tick 0 without a word said. The whole basis for not comparing is one measurement: a pair that armed on different ticks replayed clean over 98 ticks of a fixture. Nothing arms a replay from it, so the two runs spend the window between the load and the arming on different clocks, and whether that is inert on a long contributed session is outside what the fixture measured; the log carries a reported divergence at an arming point that is not yet re-derived. A file that predates the field cannot carry the value, the same class as the fixtures predating `numeric.digest`.[^record-cpp] |
| `setup.snapshot=-` | A missing snapshot. It is how a recording says its session began where a fresh boot begins, and `setup.cube` below zero is what the reader routes on. Three answers exist: an inline chunk, a sibling file named here by an older format, and `-`.[^record-cpp] |
| `bindings.keyboard=` | Nothing about the default bindings or the player's current cfg. The scancodes in the poll records mean what this line says they mean, and nothing else; a search of a recording for a default binding searches for the wrong key. |
| the poll stream | Where it should end. A poll record has no length, so a cut inside one is simply the end of the stream, reported as the ticks that reached the disk. Only the two savegame chunks detect a tear. |
| the extent | Its own length. `holds about N ticks` is read off the last sync marker, so it is approximate to one marker interval, 64 polls, and nothing in the file states the count exactly. |
| a telemetry record | Where any value sits, from one tick to the next. A telemetry record is a flat list in the digest's emission order: a fixed head, then one block per actor, then six modal values, two probe values, and the 256 game and 80 cube variables. The actor count is the scene's, so the front of the layout moves at every cube change, while the tail after the actors is fixed for a given digest version: the modal values, that version's probe values, and the two variable tables whose sizes are compile-time constants. The probe count differs by version, and a recording with no `numeric.digest` line is version 1, which three of the five fixtures in the tree are, so a tail length hard-coded from the current version reads every modal field two positions out on most of the tree's own recordings. Derive the tail from the file's declared version, anchor the modal block from the end, and re-solve each tick, as the offline reader does; a layout that is wrong by either route returns plausible integers in silence.[^control-cpp] |
| an absent header line | Before both-way comparison, nothing at all: the trait was silently unchecked. Files written before a trait was added (three of the five checked-in fixtures, for `numeric.digest`) now announce it as `mode undeclared`. |

[^record-cpp]: [SOURCES/RECORD.CPP](../../../SOURCES/RECORD.CPP), the file comment, the comment on each `REC_` constant, and `replay_report_mode`.
[^record-format-h]: [SOURCES/RECORD_FORMAT.H](../../../SOURCES/RECORD_FORMAT.H), the chunk frame comment.
[^replay-test]: [tests/automation/test_record_replay.sh](../../../tests/automation/test_record_replay.sh), the comment above the movement arm.
[^coverage-pr]: Pull request 659 on origin, the paragraph beginning "A `mode differs` mismatch may be a sibling defect"; [SOURCES/RECORD.CPP](../../../SOURCES/RECORD.CPP), `replay_covered_the_file` and the comment above it.
[^control-cpp]: [SOURCES/CONTROL.CPP](../../../SOURCES/CONTROL.CPP), `Control_StateDigest`, the actor loop and the two variable loops to `MAX_VARS_GAME` and `MAX_VARS_CUBE`; [scripts/dev/dump_recording.py](../../../scripts/dev/dump_recording.py), `tele_layout`, is the reader that anchors from the end. For the `settings.` row, the sweep table in the comment above `Control_DescribeState` and the block it writes.
