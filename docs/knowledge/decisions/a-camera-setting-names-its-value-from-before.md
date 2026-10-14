---
type: Decision
title: Every camera setting names its value from before it
description: Each cam_ row in a settings table says either SETTING_BEFORE(v), the value that switches its behaviour off, or SETTING_BEFORE_NONE for a setting that only tunes another, and a CI source check refuses a row that says neither, because the camera fixtures start from those values and a new behaviour that forgot its value would stay on in every fixture with nothing failing.
status: draft
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-14T14:00:00Z }
owner: /subsystems/camera.md
relates_to:
  - /decisions/one-auto-camera-each-behaviour-a-switch.md
sources:
  - id: settings
    resource: ../../../SOURCES/SETTINGS.H
    title: SETTINGS.H, the setting row and its before field
  - id: followcam
    resource: ../../../SOURCES/FOLLOWCAM.CPP
    title: FOLLOWCAM.CPP, the camera's settings table
  - id: check
    resource: ../../../scripts/ci/check-settings-before.py
    title: The source check
  - id: camlib
    resource: ../../../tests/automation/camlib.sh
    title: camlib.sh, CAM_PLAIN
  - id: pr-before
    resource: https://github.com/LBALab/lba2-classic-community/pull/705
    title: The before column and the settings_before console command
  - id: pr-check
    resource: https://github.com/LBALab/lba2-classic-community/pull/706
    title: The source check for camera rows
---

# Context

Every Auto camera behaviour added since it first shipped is a switch whose off value is the camera from before it. The camera fixtures measure one behaviour at a time, so they start from all of those values and switch on what they are about. That start was once a hand-kept list of settings in the fixture library, which a new setting had to be added to by someone who remembered.[^camlib]

# Decision

The value lives on the setting's own row. `SETTING_BEFORE(v)` says that `v` switches the behaviour off; `SETTING_BEFORE_NONE` says the setting only tunes another or is one the fixtures set themselves. The console's `settings_before <prefix>` sets every row with a prefix to its value from before, and the fixtures' `CAM_PLAIN` is `settings_before cam_`.[^settings][^followcam][^pr-before]

A row that left the field out read the same as `SETTING_BEFORE_NONE`, so a forgotten value failed silently: the new behaviour stayed on in every fixture. `scripts/ci/check-settings-before.py` reads the tables from the source and refuses a `cam_` row that says neither, and it fails rather than reporting clean when it finds no row or a table it cannot read. It is a source check because no host test can reach these tables without most of the engine, and the fixtures need game data, so neither runs in CI.[^check][^pr-check]

# Non-goals

- **Checking the value.** The check sees that a row says something, not that `v` really switches the behaviour off. That is the fixture's job, run against a build without the behaviour.
- **Rows outside the `cam_` prefix.** The prefix is read from `CAM_PLAIN`; other tables may carry the field and are not held to it.
- **A runtime default.** `SETTING_BEFORE` is never what a player gets; the row's default is.

[^settings]: [SOURCES/SETTINGS.H](../../../SOURCES/SETTINGS.H), `SETTING_BEFORE` and `SETTING_BEFORE_NONE`.
[^followcam]: [SOURCES/FOLLOWCAM.CPP](../../../SOURCES/FOLLOWCAM.CPP), the comment above `FollowCamSettings`.
[^check]: [scripts/ci/check-settings-before.py](../../../scripts/ci/check-settings-before.py), the file comment.
[^camlib]: [tests/automation/camlib.sh](../../../tests/automation/camlib.sh), `CAM_PLAIN`.
[^pr-before]: Pull request 705 on origin.
[^pr-check]: Pull request 706 on origin.
