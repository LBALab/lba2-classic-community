---
type: Reference
title: Camera tests, host and automation
description: The host test that covers the camera's angle, ease and blend arithmetic in CI, and the automation fixtures that drive the real engine through each Auto camera behaviour and each kind of camera zone, all of them local because they need retail data and a display.
status: draft
resource: ../../TESTING.md
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
sources:
  - id: testing
    resource: ../../TESTING.md
    title: Testing (docs/TESTING.md), section 8, the fixture table and the habits
  - id: math-test
    resource: ../../../tests/camera/test_followcam_math.cpp
    title: The arithmetic in FOLLOWCAM_MATH.H, over its whole domain
  - id: camlib
    resource: ../../../tests/automation/camlib.sh
    title: camlib.sh, the trace table and CAM_PLAIN
  - id: model
    resource: ../../../tests/automation/test_camzone_model.sh
    title: The two shapes of a camera zone
  - id: blend
    resource: ../../../tests/automation/test_camzone_blend.sh
    title: The blend into and out of a shot, against a run at 0
  - id: check
    resource: ../../../scripts/ci/check-settings-before.py
    title: The source check on camera setting rows
---

# What they pin

In CI, no retail data:

- `tests/camera/test_followcam_math.cpp` covers the arithmetic in `FOLLOWCAM_MATH.H`: the angle rule and its inverse, the shortest-way-round difference, the per-update lerp, the clock-paced ease step and the blend weights.[^math-test]
- `scripts/ci/check-settings-before.py` refuses a `cam_` setting row that does not say its value from before; it checks the source, not behaviour.[^check]

Local, a real engine headless, retail data required, in no CI workflow: the `test_*cam*.sh` fixtures in `tests/automation`, listed with what each pins in [docs/TESTING.md](../../TESTING.md) section 8. `camlib.sh` turns a run's `camtrace` into a per-frame table so a fixture asserts on the shape of a motion, and most start from `CAM_PLAIN` and switch on the behaviour they measure.[^testing][^camlib] Two of them are cited as evidence by concepts here:

- `test_camzone_model.sh` puts the hero in and out of a zone of each shape and asserts that a forced zone re-aims on every frame inside and a latching one once per entry.[^model]
- `test_camzone_blend.sh` asserts the blend's length and ends, and compares the camera's own trace fields against a run with the setting at 0, leaving out the shown view and `hid`, up to the first hidden-hero recentre.[^blend]

Run them on both data sets, the default and the US disc: three camera zones differ in their flags between the two.

What none of them pins: feel. The camera's later behaviours were each judged by a scripted play test read back from the trace, and that found faults the fixtures did not. Nor do they pin cutscenes, which have no harness route, or a message box, which stops the tick harness and was driven over the `--listen` socket instead.

[^math-test]: [tests/camera/test_followcam_math.cpp](../../../tests/camera/test_followcam_math.cpp), the test functions.
[^check]: [scripts/ci/check-settings-before.py](../../../scripts/ci/check-settings-before.py), the file comment.
[^testing]: [docs/TESTING.md](../../TESTING.md), "8. Camera fixtures".
[^camlib]: [tests/automation/camlib.sh](../../../tests/automation/camlib.sh), `CAM_PLAIN`.
[^model]: [tests/automation/test_camzone_model.sh](../../../tests/automation/test_camzone_model.sh), the header comment.
[^blend]: [tests/automation/test_camzone_blend.sh](../../../tests/automation/test_camzone_blend.sh), the header comment and the comparison against the run at 0.
