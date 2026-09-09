---
type: Format
title: Session recording file (.rec)
description: One self-framing file per session, format 14 with a read floor of 9, a text header, both savegames as framed chunks, and a binary record stream indexed by input poll.
status: draft
equivalence: tested
as_of: cc01c7ae
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T12:00:00Z }
constrains:
  - /subsystems/recording.md
verified_against:
  - /references/record-tests.md
relates_to:
  - /decisions/one-recording-file.md
  - /decisions/digest-membership.md
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
---

The layout and the versioning rule are structural. The header line table and the current version numbers are read from the commit in `as_of`.

# Evidence

`tested`: the host test covers the header readers, the binding-table round trip and the chunk frame, and the automation fixture records and replays through a real engine, with and without a tick budget, across a video, and against a recording an older engine wrote. There is no disassembly to check against; the format is new construction. The second reader, `scripts/dev/dump_recording.py`, has its own copy of the record table and no compiler to notice when it is stale.[^record-cpp]

# Layout

Little-endian throughout. Four parts, in order.

| Part | Shape |
|---|---|
| Header | `LBA2REC 14`, then `key=value` lines, then a blank line. Text, so `rec info` can diff it against a live run without a parser. At most 4096 bytes. |
| Start savegame | A chunk with op `0x70`: the state the session started from, written once between the header and the first record. |
| Record stream | One flags byte per record, then a payload whose length the reader must already know. Flushed every tick. |
| End savegame | A chunk with op `0x71`, written only when the session stopped while a scene was live. Its absence is the record of a session that did not finish, which is what a crash repro wants. |

## Chunk frame

`[op][u32 len][len bytes][u32 len][u32 magic]`. The length is known before the payload is written, so nothing is seeked back to and patched: a file cut inside a chunk is short of its tail and never claims bytes that are not there. A torn chunk has no valid tail and is refused by name rather than handed to the save loader as a short savegame. The tail is also how the end chunk is found from the end of the file. A length above 16 MiB is refused before it reaches `malloc`.[^record-format-h]

## Poll records

Flags byte `0x00` to `0x0F`, or bit 7 set. A poll where nothing changed is one byte.

| Bit | Meaning | Payload |
|---|---|---|
| 7 | tick record | `u32 tick`, `u64 digest` |
| 0 | key table changed | `u8 n`, then n times `u16 index`, `u8 value` |
| 1 | `Key` changed | `s32` |
| 2 | analog block present | `s16 rsx`, `s16 rsy`, `u32 padFirst`, `s32 mdx`, `s32 mdy`, `s32 click` |
| 3 | clock delta changed | `u32` |

The analog block is written when it differs from the last block written, not from zero, so the poll that releases a held button or stick is recorded.[^record-cpp]

## Records outside the poll encoding

Told apart by the flags byte being above `0x0F`.

| Op | Record | Payload | Cadence |
|---|---|---|---|
| `0x40` | console command | `u32 tick`, `u16 len`, `len` bytes | as issued, stamped with the tick already recorded, so stream order is execution order |
| `0x50` | keyframe | `u32 tick`, `u16 count`, `count` times `s32` | every 32 ticks; versions 9 to 11 wrote no count and exactly 23 fields |
| `0x51` | telemetry | `u32 tick`, `u16 count`, `count` times `s32`, every value the digest mixes | every tick when asked for (`--record-telemetry`, `rec start verbose`), about 2.8 KB a tick |
| `0x52` | input device | `u8`, 1 after a real key-down and 0 after the mouse, the pad or the touch overlay | on change, on top of `input.keyboard=` in the header |
| `0x60` | sync marker | `u32 "SYNC"`, `u32 poll`, `u32 tick` | every 64 polls |
| `0x70`, `0x71` | savegame chunks | the frame above | once each |

## Header lines

Grouped by prefix. A compared line is diffed against the live run when a replay starts and reported as `mode differs`; an installed line is payload the replay takes and the mode diff skips, because the digest is what compares its effect.[^record-cpp]

| Prefix | Lines | Treatment |
|---|---|---|
| none | `engine` | compared: the version string of the build that wrote the file |
| `mode.` | `headless`, `audio`, `fixed_dt`, `resolution`, `language` | compared; `verbose` installed, since telemetry does not reach the simulation |
| `build.` | `flags` | compared; `platform` recorded and skipped, because cross-platform replay is the point |
| `numeric.` | `rng`, `long_double_bits`, `digest` | compared: the arithmetic and the digest set a replay has to agree with |
| `data.` | `master` | compared: the data lineage |
| `input.` | `keyboard` | installed: the device the player was last on |
| `setup.` | `snapshot`, `reloaded`, `reload_clock` | installed: how the session started, acted on rather than matched |
| `clock.` | `timer_ref_hr`, `sim_carry`, `rng_seed` | installed: the baseline, the sub-step carry, the boot seed |
| `bindings.` | `keyboard`, `gamepad` | installed; `digest` compared |
| `state.` | `dial_obj`, `choice`, `choices`, `fade`, `blackpal` | installed: the five globals no savegame carries, all or nothing |

The comparison runs both ways. A recorded line the live run contradicts is `mode differs`, and is evidence about reproduction. A trait the live build emits that the recording lacks is `mode undeclared`, with a count line saying those dimensions were not checked; it says only that the file predates the trait, and predicts nothing. The two prefixes are kept apart so the first cannot cry wolf on a file that reproduces.[^record-cpp]

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
| `mode.fixed_dt=` | When the step was pinned. It records that the step was pinned, and at what value. Without the flag on the command line, `rec start` arms the step after the load while a `--replay` arms from the header before it; both declare `mode.fixed_dt=16` and the comparison sees no difference, so the two runs disagree about a window nothing compares.[^replay-test] |
| `bindings.keyboard=` | Nothing about the default bindings or the player's current cfg. The scancodes in the poll records mean what this line says they mean, and nothing else; a search of a recording for a default binding searches for the wrong key. |
| the poll stream | Where it should end. A poll record has no length, so a cut inside one is simply the end of the stream, reported as the ticks that reached the disk. Only the two savegame chunks detect a tear. |
| the extent | Its own length. `holds about N ticks` is read off the last sync marker, so it is approximate to one marker interval, 64 polls, and nothing in the file states the count exactly. |
| an absent header line | Before both-way comparison, nothing at all: the trait was silently unchecked. Files written before a trait was added (three of the five checked-in fixtures, for `numeric.digest`) now announce it as `mode undeclared`. |

# What a new record type costs

Four sites, and missing one fails a different way: the whitelist and the step-over chain in `replay_poll`, and the table and the decoder in `dump_recording.py`. A type registered in the chain alone works only when it happens to follow a sync marker, one poll in 64, so a fixture for a new type wants records on both sides of a marker by construction. Nothing in the stream may be sized from the live build: a payload with no count cannot grow.[^record-cpp]

[^record-cpp]: SOURCES/RECORD.CPP, the file comment, the comment on each `REC_` constant, and `replay_report_mode`.
[^record-format-h]: SOURCES/RECORD_FORMAT.H, the chunk frame comment.
[^replay-test]: tests/automation/test_record_replay.sh, the comment above the movement arm.
