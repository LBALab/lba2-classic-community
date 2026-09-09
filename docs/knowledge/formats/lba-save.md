---
type: Format
title: Savegame file (.lba)
description: One byte of layout version and compression bit, the cube, the player's name, then a 19200-byte thumbnail and a field-by-field game context that are LZSS-compressed as one block when the bit is set; layout 36 on every host, with three structs pinned to their 32-bit retail width.
status: draft
equivalence: partial
as_of: 9f3750f5
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
constrains:
  - /subsystems/save.md
verified_against:
  - /references/save-tests.md
relates_to:
  - /formats/rec.md
  - /decisions/the-save-version-stays-36.md
  - /decisions/compressed-saves-round-trip-byte-for-byte.md
sources:
  - id: savegame-doc
    resource: ../../SAVEGAME.md
    title: Savegame system (docs/SAVEGAME.md), the field map and the hardening table
  - id: savegame-cpp
    resource: ../../../SOURCES/SAVEGAME.CPP
    title: SAVEGAME.CPP, SaveGame, LoadGame, SaveContexte and LoadContexte
  - id: savegame-wire-h
    resource: ../../../SOURCES/SAVEGAME_WIRE.H
    title: SAVEGAME_WIRE.H, the three wire mirrors and their size locks
  - id: common-h
    resource: ../../../SOURCES/COMMON.H
    title: COMMON.H, NUM_VERSION, SAVE_COMPRESS and MASK_NUM_VERSION
  - id: wire-plan
    resource: ../../plan/SAVE_WIRE_PLAN.md
    title: Save wire-format plan, the section map and the four decisions
  - id: probe
    resource: ../../../scripts/save_probe.py
    title: The offline reader that walks both strides
---

docs/SAVEGAME.md owns the field map, offset by offset, the `ListVarGame` index table, the behaviour constants and the hardening table, and docs/plan/SAVE_WIRE_PLAN.md owns the section map with its oracle per section; neither is repeated here. This concept holds the envelope, the versioning rule and what the bytes do not say. The layout is structural; the reader's trial order and the test coverage are read at `as_of`.

# Evidence

`partial`. The three wire structs are under host test: a golden decode against a hand-derived layout, a writer round trip, a converter fuzz and compile-time size locks. The bounds helpers, the thumbnail capture and the LZ decoder each have a host test of their own. The whole payload's round trip through `SaveContexte` and `LoadContexte` is under no host test, a gap docs/SAVEGAME.md names, and the harness that loads fifty retail saves through the engine needs retail data and runs locally rather than in public CI. There is no disassembly to check against: the writer and the reader are Adeline's C++, and what the retail engine wrote is known from the corpus.[^savegame-doc]

# Layout

Little-endian throughout. Four parts.

| Part | Shape |
|---|---|
| Header | The version byte; the cube as a 32-bit integer; the player name, NUL-terminated and bounded on read by `MAX_SIZE_PLAYER_NAME`; and, when the compression bit is set, the decompressed size of everything after as a 32-bit integer. The header is never compressed.[^savegame-cpp] |
| Thumbnail | 19200 bytes: 160 by 120 palette indices, scaled from `Log` at save time and remapped to the game palette. No palette is stored.[^savegame-cpp] |
| Game context | `SaveContexte`, field by field through the byte, word and long writers, with the three pointer-bearing structs converted through wire mirrors; the order and widths are the two tables named above.[^wire-plan] |
| Valid-position tail | `ValidePos` and `LastValidePos`; when the first is zero, `ValideCube`, the buffer's size bounded by `SIZE_BUFFER_VALIDE_POS`, and the buffer.[^savegame-cpp] |

When the compression bit is set, everything from the thumbnail to the end of the tail is one LZSS block, `Compress_LZSS` on the way out and `ExpandLZ` with a minimum block of 2 on the way in.

## The version byte

The low seven bits are `NUM_VERSION`, 36, the layout revision; the high bit is `SAVE_COMPRESS`; `MASK_NUM_VERSION` strips the bit for a layout comparison. The two bytes seen in practice are 0x24 and 0xA4.[^common-h]

## The per-object record

A 140-byte scalar prefix written field by field, then the object's `T_OBJ_3D` without `CurrentFrame` as a 136-byte wire mirror, 276 bytes in all. `T_EXTRA` is 68 bytes on the wire and `S_PART_FLOW` 60; the native structs are 164, 80 and 64 on a 64-bit host, which is what the mirrors exist to hide. Four pointer slots the reader discards are written as zero; two pointer-typed frame offsets keep their low 32 bits; `PtrZoneClimb` elsewhere in the context is written as a 32-bit value and does not round-trip a wider pointer.[^savegame-wire-h]

## Reading the stride

A layout-36 file carries no signal for which stride wrote it. The reader reads the object array at the wider native stride first, validates every object's `IndexFile3D` against a loose bound, and on failure rewinds and reads at the wire stride; the one flag that trial sets then selects the extra and flow widths, on the assumption that one program wrote the whole file. The order is [a decision](/decisions/the-save-version-stays-36.md) and the reverse order was measured to crash.[^savegame-cpp]

# Versioning

| Change | Moves | Rule |
|---|---|---|
| a field inserted, removed or reordered in the context | `NUM_VERSION` | every existing file misaligns; the reader branches on the version, and a release build only carries the branches for 35 and 36 |
| a field appended per object | `NUM_VERSION` | the two most recent revisions each did this, `SampleAlways` at 35 and `SampleVolume` at 36, and the reader branches on the version around the read |
| the host's word size | nothing | the version byte is not an ABI tag; the writer emits the 32-bit layout on every host and the reader discovers a legacy native file by trial |
| compression | the high bit only | independent of the layout, chosen per writer |

# What it does not tell you

| Field or property | What it does not say |
|---|---|
| the version byte | Which program wrote the payload. The retail 32-bit engine and 64-bit builds before the wire writer both wrote 36, with object records 28 bytes apart, and nothing in the header separates them. |
| the cube in the header | Where the hero is. The positions are in the context, and a checksum mismatch overrides them with the scene start. It is what the light loader reads, and all it reads besides `ListVarGame`. |
| the player name | The file's name, or who saved. The automatic files carry `CURRENT` and `AUTOSAVE`, the harness snapshot carries `rec`, and the menu derives a filename from a typed name that the file does not record. Community tools read the four bytes after the version byte as an hour and padding; the engine reads a cube index. |
| the thumbnail | Its colours. Indices only, with no palette in the file; a viewer needs the game palette. An autosave's thumbnail is captured under a black palette and is still the scene, because indices do not darken. |
| the checksum | That the file is damaged. It is the scenario's checksum; a mismatch means a different build of the scripts, and the reader takes the short path rather than refusing. |
| the decompressed size | That the block is intact. The staging check compares geometry, not content, and the LZ stream has no checksum of its own. |
| the per-object stride | Itself. It is not stored, the reader infers it, and a one-object file validates under both strides because the first object's `IndexFile3D` sits in the stride-independent prefix; see the decision for what that means. |
| the context's offsets | Their position in the file. Offsets in the reference doc are relative to the context, and the header's variable-length name puts the context at a different file offset per save. |

[^savegame-doc]: docs/SAVEGAME.md, "File format", "Version byte on disk", "32-bit vs 64-bit and pointers", "Format hardening" and "Still future / larger change".
[^savegame-cpp]: SOURCES/SAVEGAME.CPP, `SaveGame` from the version byte to the valid-position tail, `LoadGame`, and the stride trial and its comment in `LoadContexte`.
[^savegame-wire-h]: SOURCES/SAVEGAME_WIRE.H, the file comment and the three size locks.
[^common-h]: SOURCES/COMMON.H, the three `#define` lines for the version byte.
[^wire-plan]: docs/plan/SAVE_WIRE_PLAN.md, "End-to-end section map".
[^probe]: scripts/save_probe.py, the module docstring.
