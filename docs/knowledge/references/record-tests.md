---
type: Reference
title: Recording tests
description: The host test for the recording format's readers and frame, and the automation fixtures that record and replay through a real engine; the attesters behind the .rec Format.
status: draft
resource: ../../../tests/automation/test_record_replay.sh
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T09:00:00Z }
sources:
  - id: host
    resource: ../../../tests/record_format/test_record_format.cpp
    title: The host test
  - id: replay
    resource: ../../../tests/automation/test_record_replay.sh
    title: The record-and-replay fixture
---

# Host

`tests/record_format` links the buffer-only framing in `RECORD_FORMAT.CPP` and covers the header field readers, the binding-table round trip and the chunk frame, including the tail check that refuses a torn chunk. It runs with the host tests in CI.[^host]

# Automation

These need retail data, so they run locally rather than in CI. `tests/automation/test_record_replay.sh` records a session and replays it with `--tick` and without, replays `recordings/legacy-v10.rec` as an older engine wrote it, reads the file back with `scripts/dev/dump_recording.py`, records across a video behind a poll-count guard so an arm that played no video cannot pass, replays a recording that opens the in-game menu through the menu, requiring every tick, checks that the step's arming tick is carried and never reported on a pair that reproduces, and records a session from a fresh boot to assert it carries no starting state and replays into one. `test_record_analog.sh` and `test_record_input_device.sh` cover the analog block and the device record.[^replay]

Two arms are deliberately unlike the rest, and tidying either would remove what it covers. The movement arm records without `--fixed-dt`, so its recording arms the step after the load while the replay arms from the header before it; it is the suite's only comparison across mismatched step arming, and pinning it would blind that. The loose arm runs a genuinely host-sampled recording through a fade and asserts termination only, because two host-sampled runs do not reach identical state and a comparison there would be flaky by construction.[^replay]

[^host]: tests/record_format/test_record_format.cpp.
[^replay]: tests/automation/test_record_replay.sh, the header comment.
