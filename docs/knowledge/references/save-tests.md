---
type: Reference
title: Save tests, host and retail-gated
description: The host tests behind the save file, covering the three wire structs, the bounds helpers, the thumbnail capture, the name gate and the LZ decoder, and the retail-gated drivers that load the corpus and the legacy fixtures through the engine; the attesters behind the .lba Format.
status: draft
resource: ../../../tests/save_wire/test_save_wire_from32.cpp
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
sources:
  - id: from32
    resource: ../../../tests/save_wire/test_save_wire_from32.cpp
    title: The layout anchor and golden decode
  - id: to32
    resource: ../../../tests/save_wire/test_save_wire_to32.cpp
    title: The writer round trip
  - id: fuzz
    resource: ../../../tests/save_wire/test_save_wire_fuzz.cpp
    title: The converter fuzz
  - id: bounds
    resource: ../../../tests/savegame/test_load_bounds.cpp
    title: The bounds helpers
  - id: name
    resource: ../../../tests/savegame/test_save_name_validation.cpp
    title: The reserved-shape check on a typed name
  - id: thumbnail
    resource: ../../../tests/save_thumbnail/test_save_thumbnail.cpp
    title: The thumbnail capture at every width
  - id: lz
    resource: ../../../tests/SYSTEM/test_lz.cpp
    title: The decoder's golden vectors
  - id: corpus
    resource: ../../../tests/savegame/corpus/run_harness.py
    title: The corpus driver
  - id: fallback
    resource: ../../../tests/savegame/corpus/native_fallback_check.py
    title: The legacy fixtures check
  - id: probe
    resource: ../../../scripts/save_probe.py
    title: The offline reader
  - id: runner
    resource: ../../../scripts/dev/run-savegame-corpus.sh
    title: The make target's script
---

# What they pin

Host tests, no retail data, run in CI with the rest of the host tests:

- `tests/save_wire` pins the 32-bit retail layout of the three pointer-bearing structs against hand-derived byte vectors, independently of the wire structs themselves; proves the readers decode that layout; proves the writers are their exact inverses, with the discarded pointer slots emitted as zero; and fuzzes write, read, write for byte idempotency over thousands of random structs. The header's size locks fail the build on a drift.[^from32][^to32][^fuzz]
- `tests/savegame` pins the bounds helpers the reader uses, and the reserved-shape rule that keeps a typed save name from colliding with a generated one.[^bounds][^name]
- `tests/save_thumbnail` pins the 160 by 120 capture at every supported width, after a wide mode overflowed the old scratch buffer.[^thumbnail]
- `tests/SYSTEM/test_lz.cpp` pins the decoder with golden vectors that `scripts/save_probe_lz_selftest.py` mirrors for the offline tool.[^lz]

Retail-gated, run locally or in the gated job, never in public CI:

- The corpus driver runs `--save-load-test` over every committed save under the automatic reader and the forced wire reader, records the outcome per save, and checks the offline probe's prediction against the engine's outcome.[^corpus][^runner]
- The fallback check drives the two native-layout fixtures and a sample of the wire corpus and asserts each outcome, including the migration warning.[^fallback]
- The offline reader parses a save with no engine, decompresses through the `save_decompress` tool, walks the whole context under both stride hypotheses, and reports which one the engine will take.[^probe]

What none of them pins: a whole-payload round trip through `SaveContexte` and `LoadContexte` on the host, and the byte identity of a recompressed save against its original.

[^from32]: [tests/save_wire/test_save_wire_from32.cpp](../../../tests/save_wire/test_save_wire_from32.cpp), the file comment.
[^to32]: [tests/save_wire/test_save_wire_to32.cpp](../../../tests/save_wire/test_save_wire_to32.cpp), the file comment.
[^fuzz]: [tests/save_wire/test_save_wire_fuzz.cpp](../../../tests/save_wire/test_save_wire_fuzz.cpp), the file comment.
[^bounds]: [tests/savegame/test_load_bounds.cpp](../../../tests/savegame/test_load_bounds.cpp).
[^name]: [tests/savegame/test_save_name_validation.cpp](../../../tests/savegame/test_save_name_validation.cpp), the file comment.
[^thumbnail]: [tests/save_thumbnail/test_save_thumbnail.cpp](../../../tests/save_thumbnail/test_save_thumbnail.cpp), the file comment.
[^lz]: [tests/SYSTEM/test_lz.cpp](../../../tests/SYSTEM/test_lz.cpp); [scripts/save_probe_lz_selftest.py](../../../scripts/save_probe_lz_selftest.py).
[^corpus]: [tests/savegame/corpus/run_harness.py](../../../tests/savegame/corpus/run_harness.py), the module docstring; [tests/savegame/corpus/README.md](../../../tests/savegame/corpus/README.md).
[^fallback]: [tests/savegame/corpus/native_fallback_check.py](../../../tests/savegame/corpus/native_fallback_check.py), the module docstring.
[^probe]: [scripts/save_probe.py](../../../scripts/save_probe.py), the module docstring.
[^runner]: [scripts/dev/run-savegame-corpus.sh](../../../scripts/dev/run-savegame-corpus.sh), behind `make savegame-corpus`.
