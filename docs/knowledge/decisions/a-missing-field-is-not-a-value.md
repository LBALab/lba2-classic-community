---
type: Decision
title: The replay reader does not substitute a value for a field the file does not carry
description: Two header fields answer "the file does not say" with a sentinel the reader routes on rather than a default it fills in; a missing device flag leaves the live flag alone, a cube below zero means the session began before there was a game and replays as a fresh boot, and the shipped cost of substituting was a replay that booted into a game that never started and crashed before its first tick.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T21:00:00Z }
owner: /subsystems/recording.md
relates_to:
  - /formats/rec.md
  - /subsystems/input.md
  - /subsystems/save.md
  - /decisions/one-recording-file.md
  - /decisions/digest-membership.md
sources:
  - id: record-cpp
    resource: ../../../SOURCES/RECORD.CPP
    title: RECORD.CPP, the device record's comment, the reader's -1, and the setup.cube read before the snapshot chunk
  - id: pr-666
    resource: https://github.com/LBALab/lba2-classic-community/pull/666
    title: fix(record), a replay must not boot into a game that never started
  - id: device-test
    resource: ../../../tests/automation/test_record_input_device.sh
    title: The fixture whose arms read the save folder rather than the exit code
  - id: legacy
    resource: ../../../tests/automation/recordings/legacy-v13.rec
    title: A recording from before the device field existed
  - id: gamemenu
    resource: ../../../SOURCES/GAMEMENU.CPP
    title: GAMEMENU.CPP, the gate in ChoosePlayerName
---

# Context

Two fields, two histories, one shape.

The device. `ChoosePlayerName` offers a text field when the last input came from a physical keyboard and a datetime stem otherwise. The flag behind that, `LastInputWasKeyboard`, is set by one real key-down event and cleared by the mouse, the pad and the overlay; it is a device fact the game reads rather than input, it is in no digest, and no recording carried it. So a replay of a session that typed a save name took the other branch and wrote a differently named file, and nothing reported it: a save slot is in none of the categories the digest hashes, so the run diverged and exited 0. A headless run never reaches the typed-name screen at all, which is why fixtures that need it set the flag through the console.[^device-test][^gamemenu]

The start state. `--record` arms after `InitGame`, which sets the cube to -1 to force the first cube change, so a session recorded from a fresh boot has no scene. The writer saved a snapshot there anyway, a 2705-byte husk whose cube is -1 against ten to twelve kilobytes for a real save. Once a replay without `--load` booted into the recording's own starting state, it loaded that, and the loader does not refuse a negative cube: it walks `ChangeCube` off the end of the scene arrays honouring it, and the run died before its first tick. An explicit `--load` never asks the recording, which is why it went unnoticed.[^pr-666]

# Decision

The reader asks whether the file says, and when it does not, it does nothing rather than something plausible.

For the device, the value is carried in two halves, the header's `input.keyboard=` for the value the session started on and a `REC_DEVICE` record for each change; the header alone freezes a session that switched devices and the records alone leave the session that never switched carrying nothing. The reader holds -1 for "the file does not say", installs the flag only when the value is non-negative, and guards the save-and-return path the same way. The design that was first agreed defaulted a silent file to zero and wrote that down as a known limit; it built and passed every same-binary arm, and it was wrong, because a format-13 recording reaches the typed-name screen by setting the flag from the console, a command the recording carries, so a reader that forced a default stamped on the file's own instruction, replayed clean, exited 0, and wrote the wrong name.[^record-cpp][^legacy]

For the start state, the reader reads `setup.cube` before the snapshot chunk and returns "no snapshot" on a negative value: not the load, which is the crash, and not the refusal, since nothing is missing. A fresh boot is the state such a session began from and it replays as one. The writer stores no snapshot without a live scene, on the same predicate the recorder already routes on, and the header says `-`. The reader half is what keeps the files recorded before the fix replayable, and those are the files that matter.[^record-cpp][^pr-666]

Declining is available in both cases only because the field can be absent in a way a value cannot be: a `key=value` line is distinguishable from a recorded zero, where a positional field has two answers; and a cube below zero is a number no live game has.

# Why the cost is invisible

Substituting a value is expensive not because it is wrong but because being wrong is invisible to the oracle. The device default writes a save under the wrong name, and a save slot is in no digest category, so the replay reports that it reproduced. The start-state husk crashed, which is visible, but only on the path that asks the recording; every run given `--load` passed. A reader that fills silence with a value produces a run the verdict cannot fault.[^record-cpp]

# Non-goals

- **Guessing.** No default for a file that predates a field; the behaviour the run already had is the answer, not a value.
- **Telling the devices apart.** The payload says keyboard or not; "not keyboard" is three devices and the record does not name them. The byte carries no count and cannot grow; a later need wants a record type of its own.[^record-cpp]
- **Carrying the keymap.** A replayed keystroke reproduces the key, not the character, because text entry resolves the recorded scancode through the replaying machine's layout; that gap is open and is not this field's.
- **Refusing a from-boot recording.** The refusal is for a torn or unreadable file. A session that began before there was a game is whole, and it says so with `-`.

[^record-cpp]: [SOURCES/RECORD.CPP](../../../SOURCES/RECORD.CPP), the comment above `REC_DEVICE`, `s_device` and `s_deviceSaved` with their -1 defaults and guarded installs, and the `setup.cube` read with its comment in `Record_ArmedReplayLoad`.
[^pr-666]: Pull request 666 on origin, merged as d9cf303d, "What happens" and "The fix, at both ends".
[^device-test]: [tests/automation/test_record_input_device.sh](../../../tests/automation/test_record_input_device.sh), the header comment.
[^legacy]: [tests/automation/recordings/legacy-v13.rec](../../../tests/automation/recordings/legacy-v13.rec), the arm the rejected default fails.
[^gamemenu]: [SOURCES/GAMEMENU.CPP](../../../SOURCES/GAMEMENU.CPP), the `LastInputWasKeyboard` test in `ChoosePlayerName`.
