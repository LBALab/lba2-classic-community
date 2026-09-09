---
type: Decision
title: The save version stays 36, so the reader tries the wider stride first
description: The portable writer emits the retail layout under the retail version number rather than a 37, so the version byte cannot say which stride wrote a file; the reader reads the wider native record first and falls back to the wire, because the other order validated the wrong layout on real saves and crashed.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
constrains:
  - /formats/lba-save.md
relates_to:
  - /decisions/legacy-port-64-saves-warn-and-migrate.md
  - /subsystems/save.md
sources:
  - id: wire-plan
    resource: ../../plan/SAVE_WIRE_PLAN.md
    title: Save wire-format plan, "Decisions (resolved 2026-07-07)" and "Decision 1"
  - id: savegame-cpp
    resource: ../../../SOURCES/SAVEGAME.CPP
    title: SAVEGAME.CPP, the comment above the stride trial in LoadContexte, and LoadContexteReadObjectsAtStride
  - id: savegame-doc
    resource: ../../SAVEGAME.md
    title: Savegame system (docs/SAVEGAME.md), "Reader (native-first)"
  - id: pr
    resource: https://github.com/LBALab/lba2-classic-community/commit/3d54baca
    title: fix(savegame), read native stride first to avoid a legacy-save crash
---

# Context

Before the portable writer, a 64-bit build wrote layout 36 with its native structs: 304 bytes per object against retail's 276, 80 per extra against 68, 64 per flow against 60. The plan put two options up: emit true retail bytes under the same 36 and accept that the version byte cannot tell a retail file from one of those, or bump to 37 so the reader dispatches on the byte and the 36 path becomes a legacy branch. The plan leaned to the bump. The maintainer chose 36: one layout on disk, always retail-shaped, and the files a pre-release port had written treated as a migration corpus rather than a second format.[^wire-plan]

The plan's consequence was a canonical-first reader: read the wire stride, fall back to native on validation failure. Implemented that way it crashed on a real legacy save. Reading a 276-byte record out of 304-byte data stops inside bytes that are still valid, so a native file with several objects passed the wire validation, committed the wrong layout, and dereferenced garbage in `InitLoadedGame`; the offline probe confirms such files walk clean under both hypotheses. The order was reversed in the same PR.[^pr]

# Decision

Stay at 36, and read the wider record first. A native-width read of wire data overruns into the next record, so a genuine wire file fails the per-object validation reliably and is retried at the wire stride; a legacy native file validates on the first try. The one flag that trial sets selects the extra and flow widths, on the assumption that a single program wrote the file. Measured on the way in: 237 retail-wire saves loaded with no misread, and the legacy fixtures, including a one-object native file, loaded correctly. `LBA2_SAVE_LOAD_ABI` forces either path for an audit and is the seam the tests use.[^savegame-cpp]

# What the trial cannot tell

The validation is a loose bound on every object's `IndexFile3D`, and the first object's copy of that field sits in the 140-byte scalar prefix, which is the same at both strides. A file with one object therefore validates under both hypotheses whichever is tried first, and with native tried first it commits native. Under the old order the blind spot was a one-object native file, and the fix moved it to a one-object wire file: a retail save from a cube holding only the hero would be read at the wrong stride, and the range check on the patch count that follows the array is the next thing that would notice. This is derived from the validator's code and not measured: no save in either corpus has fewer than three objects, and whether retail writes a one-object file at all is not established here.[^savegame-cpp]

# Non-goals

- **A 37.** The bump would have made the dispatch clean and the trial unnecessary; it was declined, and the trial is the price.
- **A stride tag in the file.** Any tag is a layout change, which is the bump under another name.
- **The predictive sniff.** The heuristic that guessed the stride from the patch count ahead of the read, with its off-by-two constants, was deleted rather than kept beside the trial.[^savegame-doc]
- **A host test of the fallback.** The legacy path is checked by a retail-gated script over the two native fixtures, not by a host test; see [the legacy decision](/decisions/legacy-port-64-saves-warn-and-migrate.md).

[^wire-plan]: docs/plan/SAVE_WIRE_PLAN.md, "Decisions (resolved 2026-07-07)", item 1, and "Decision 1: version numbering".
[^savegame-cpp]: SOURCES/SAVEGAME.CPP, the comment beginning "genuine wire save reliably fails native validation" in `LoadContexte`, and the validation loop at the end of `LoadContexteReadObjectsAtStride`.
[^savegame-doc]: docs/SAVEGAME.md, "Reader (native-first)" and "Consequence".
[^pr]: Commit 3d54baca on origin, "fix(savegame): read native stride first to avoid a legacy-save crash", in the history of pull request 386.
