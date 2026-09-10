---
type: Decision
title: A compressed save round-trips byte for byte, at the file level
description: The contract for a compressed save is identity of the whole file after a load and a save, not identity of the decompressed payload plus a separate compressor test, so a difference in the LZSS encoder's output from retail's is a fidelity bug to fix; nothing in the tree asserts the contract yet.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
owner: /subsystems/save.md
constrains:
  - /formats/lba-save.md
relates_to:
  - /decisions/the-committed-corpus-is-the-oracle.md
sources:
  - id: wire-plan
    resource: ../../plan/SAVE_WIRE_PLAN.md
    title: Save wire-format plan, decision 4 and "Compressed byte-exactness hinges on encoder fidelity"
  - id: lzss-cpp
    resource: ../../../SOURCES/LZSS.CPP
    title: LZSS.CPP, Compress_LZSS, the original binary-tree encoder
  - id: lz-cpp
    resource: ../../../LIB386/SYSTEM/LZ.CPP
    title: LZ.CPP, ExpandLZ, the decoder
  - id: savegame-cpp
    resource: ../../../SOURCES/SAVEGAME.CPP
    title: SAVEGAME.CPP, the one call to Compress_LZSS in SaveGame
---

# Context

Every save in the committed corpus is compressed, so a byte-identical round trip of the corpus is only reachable if the engine's encoder reproduces the retail encoder's token stream. The encoder in the tree is Adeline's own binary-tree LZSS, deterministic per call, and the decoder's window and length widths match what the community tools document; whether the encoder's tie-breaks match what the retail executable shipped with is the open question. Two contracts were on the table: identity of the whole file, or identity of the decompressed payload with the compressor tested separately, which isolates the serialiser from the encoder.[^wire-plan]

# Decision

The whole file. A compressed save loaded and saved again is the same bytes, and any divergence is an encoder-fidelity bug to fix in `Compress_LZSS`, never a reason to compare the payload instead. The corpus is the oracle for it.[^wire-plan]

# Where it stands

Declared and not asserted. The tree carries no test or script that compresses a corpus payload and compares it with the original file: the gated round-trip test the campaign planned was not built, `Compress_LZSS` has no caller outside the engine, and the plan's own note that the first phase would report empirically which contract holds has no report beside it. What is asserted is weaker: the three wire converters round-trip, a save written by the port loads and re-saves stably, and the decoder's golden vectors pass. Whether the encoder is byte-faithful to retail is therefore unknown at `as_of`, and the first measurement is one save: decompress it, recompress the payload, compare.[^savegame-cpp][^lz-cpp]

# Non-goals

- **A payload-only contract.** It was the recommended fallback and it was declined; a reader who finds the encoder unfaithful fixes the encoder.
- **Changing the compressor for ratio or speed.** Its output is part of the format by this decision.
- **A decompression-only oracle.** The corpus proves the decoder and the reader; it proves the writer only through this contract.

[^wire-plan]: [docs/plan/SAVE_WIRE_PLAN.md](../../plan/SAVE_WIRE_PLAN.md), "Decisions (resolved 2026-07-07)", item 4, "Decision 4 (surfaced): compressed round-trip contract", and the paragraph beginning "Compressed byte-exactness hinges on encoder fidelity".
[^lzss-cpp]: [SOURCES/LZSS.CPP](../../../SOURCES/LZSS.CPP), `Compress_LZSS`.
[^lz-cpp]: [LIB386/SYSTEM/LZ.CPP](../../../LIB386/SYSTEM/LZ.CPP), `ExpandLZ`; [tests/SYSTEM/test_lz.cpp](../../../tests/SYSTEM/test_lz.cpp) carries the decoder's golden vectors.
[^savegame-cpp]: [SOURCES/SAVEGAME.CPP](../../../SOURCES/SAVEGAME.CPP), the `Compress_LZSS` call in `SaveGame`.
