---
type: Decision
title: Legacy 64-bit saves load, warn, and migrate on the next save
description: A file a pre-portable 64-bit build wrote still loads through the native stride, the engine says so in the log, and the next save rewrites it in the retail layout; the path is kept minimal and marked for removal because those files are local artifacts of a pre-release port, not an installed base.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
owner: /subsystems/save.md
relates_to:
  - /decisions/the-save-version-stays-36.md
  - /formats/lba-save.md
sources:
  - id: wire-plan
    resource: ../../plan/SAVE_WIRE_PLAN.md
    title: Save wire-format plan, decision 2
  - id: savegame-cpp
    resource: ../../../SOURCES/SAVEGAME.CPP
    title: SAVEGAME.CPP, the warning after a native read validates
  - id: fixtures
    resource: ../../../tests/savegame/corpus/saves/native_port64/README.md
    title: The two native-layout fixtures and how they were made
  - id: fallback-check
    resource: ../../../tests/savegame/corpus/native_fallback_check.py
    title: The retail-gated check over the fixtures
---

# Context

The port's 64-bit builds wrote saves for months with their native struct widths, under the retail version number. Those files exist only on the machines of people who built the port before the wire writer landed; there is no release with them in it. The question was whether to keep reading them at all.[^wire-plan]

# Decision

Keep the native read, quarantined behind the stride trial, and make it visible: when the native read is the one that validates and no audit forced it, log a warning that a legacy 64-bit save was loaded and will be migrated to the portable format the next time it is saved. The writer then emits the wire layout whatever the reader found. The path is a courtesy for local files, kept as small as it can be and marked for removal; its coverage is a focused detect-warn-migrate check over two fixtures, a multi-object file saved by a pre-fix build and a one-object file cut from it, run where retail data is.[^savegame-cpp][^fixtures]

# Non-goals

- **A compatibility matrix.** Pre-release files get one path and one check, not a table of builds.
- **Keeping it.** The plan names the path as removable once those local files have been rewritten; nothing in the tree depends on it beyond the two fixtures.
- **Detecting a legacy file any other way.** The version byte cannot, by [the decision above](/decisions/the-save-version-stays-36.md), and no second signal was added.
- **A host test.** The check drives the engine over real files and needs retail data, so it runs locally and in the retail-gated job, not in public CI.[^fallback-check]

[^wire-plan]: [docs/plan/SAVE_WIRE_PLAN.md](../../plan/SAVE_WIRE_PLAN.md), "Decisions (resolved 2026-07-07)", item 2.
[^savegame-cpp]: [SOURCES/SAVEGAME.CPP](../../../SOURCES/SAVEGAME.CPP), `LoadContexte`, the `Log_Warn` after `LoadContexteReadObjectsAtStride` returns 1 on the native try.
[^fixtures]: [tests/savegame/corpus/saves/native_port64/README.md](../../../tests/savegame/corpus/saves/native_port64/README.md).
[^fallback-check]: [tests/savegame/corpus/native_fallback_check.py](../../../tests/savegame/corpus/native_fallback_check.py), the module docstring.
