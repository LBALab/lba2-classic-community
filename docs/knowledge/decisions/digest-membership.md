---
type: Decision
title: Every digest field declares why a replay can establish it
description: Four membership classes, passed as an argument to the mixing macro, so a field cannot be hashed without saying whether the load restores it, the file carries it, nothing establishes it, or it is not state at all.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T09:00:00Z }
constrains:
  - /subsystems/recording.md
  - /formats/rec.md
sources:
  - id: plan
    resource: ../../plan/DIGEST_MEMBERSHIP.md
    title: The state digest, a membership rule and what it costs
  - id: control-h
    resource: ../../../SOURCES/CONTROL.H
    title: CONTROL.H, the class constants and their comment
  - id: review
    resource: ../../plan/RECORDER_OBSERVER_REVIEW.md
    title: The recorder as an observer, "The digest needs a stated membership rule"
---

# Context

Five globals that no savegame carries (`NumObjDial`, `GameChoice`, `GameNbChoices`, `FlagFade`, `FlagBlackPal`) were hashed and compared, so a recording started after any dialogue mismatched at tick 0 for its whole length with nothing having diverged. Seven of nine contributed recordings did.[^plan] `first hash mismatch 0` is the loudest thing a replay says, and on one macOS session it hid a real divergence that began past tick 3100.[^review]

A name-grep over the save code is not the oracle for which fields are safe. It clears six hero fields for the wrong reason, because the save writes the whole struct, and condemns four that come back with the cube load (`Island`, `CubeMode`, `NbObjets`, `NbZones`).[^plan]

# Decision

For every field the digest touches, exactly one of these is true, and the class is an argument to the mixing macro rather than a comment beside it. There is no way to add a field without answering.[^control-h]

| Class | Meaning | Behaviour |
|---|---|---|
| `DIGEST_SAVED` | the replay's load restores it, whether the save carries it or the cube data does | compared; a mismatch is a divergence |
| `DIGEST_CARRIED` | the load does not restore it, so the file carries it and the replay installs it | compared |
| `DIGEST_LOOSE` | nothing establishes it in the replay | collected and named, never compared |
| `DIGEST_PROBE` | not state: a measurement of how the run got here, such as the draw count | collected and named, never compared |

The five globals are class 2, not class 3: they reach the simulation, so the file carries them as `state.` header lines and the replay installs them all or nothing, on the borrow-and-return the binding tables already use. `DIGEST_LOOSE` is a diagnosis, not a destination: a field there wants carrying.[^plan]

Neither carrying the five nor repairing a class-3 field into class 2 moves `CONTROL_DIGEST_VERSION`, because the set of hashed fields did not change, and neither moves `REC_VERSION`, because the lines only add. An unknown digest version above the build's is refused by name; every known lower one is reproduced exactly, since fields are appended and never interleaved.[^plan]

# Non-goals

- **Dropping the five from the hash.** That removes the false alarm by removing the ability to see a real divergence, and cannot help a recording that already exists.
- **Grouped per-tick hashes.** Priced at 40 bytes against 19.8 for a plain tick, a 202% increase, for a diagnosis the keyframe and telemetry already give.
- **A fourth digest class for environment assertions** (save slots on disk, UI cursors). Right idea, wrong place: none of it is in the digest, so it belongs to the header and the arm-time check, where `mode differs` already lives.
- **Bumping a version to signal the repair.** The cost of not bumping is an older build reading a newer file and printing five spurious `mode differs` lines, which is noise in one direction against refusing the file in the other.

[^plan]: docs/plan/DIGEST_MEMBERSHIP.md, "The classes", "The five globals are class 2" and "The versioning decision".
[^control-h]: SOURCES/CONTROL.H, the `DIGEST_` constants and the comment above them.
[^review]: docs/plan/RECORDER_OBSERVER_REVIEW.md, "The digest needs a stated membership rule".
