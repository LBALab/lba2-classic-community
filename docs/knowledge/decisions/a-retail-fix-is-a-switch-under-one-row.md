---
type: Decision
title: A retail fix is a switch of its own, and one row turns them all off
description: A fix to a bug the 1997 release has is on by default, has its own fix_ setting, and sits under one master row, Fix retail bugs (fix_retail), whose No is the original's rules including fixes added later; a design change is an enhancement with its own setting, off by default and outside the row.
status: draft
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-19T20:00:00Z }
owner: /subsystems/console.md
relates_to:
  - /decisions/a-camera-setting-names-its-value-from-before.md
  - /decisions/a-missing-field-is-not-a-value.md
sources:
  - id: issue
    resource: https://github.com/LBALab/lba2-classic-community/issues/760
    title: Retail bug fixes, a switch per fix that changes play
  - id: pr
    resource: https://github.com/LBALab/lba2-classic-community/pull/759
    title: The first retail fix, a direction held through the found-item popup
  - id: module
    resource: ../../../SOURCES/RETAILFIX.H
    title: RETAILFIX.H, the master row, the fixes and the arithmetic
  - id: plan
    resource: ../../plan/RETAIL_FIX_PLAN.md
    title: The plan PR 759 built, with the backlog sorted into buckets
---

# Context

Some reported bugs are in the original game: the 1997 CD releases (DOS and Windows 95) have them, which a 90s build or the 1997 source shows. The 2022 Classic release is a later port and not the reference. Fixing such a bug changes play against the original, and the project keeps the original reachable (AGENTS.md, Principle 3). The first case was a direction held through the found-item popup leaving Twinsen stuck.[^pr] The tester who reported most of them did not want a menu switch per fix; speedruns use the original executables, so exact parity is not a goal here.[^issue]

# Decision

A retail fix has its own setting, `fix_<name>` in the console and `RetailFix<Name>` in `lba2.cfg`, on by default. One master row decides which apply: **Fix retail bugs** in Options → Advanced options, `fix_retail`, `RetailFixes`.[^module]

- **No (0)** turns every fix off, including fixes added in later versions. It is a stored value, not a preset, so a player who chose the original's rules keeps them across updates.
- **Yes (1)** turns every fix on, the default.
- **Custom (2)** lets each fix's own setting decide. The menu offers it only once a fix is off its default; changing a fix at runtime moves the master to Custom, so the change is never silently overridden. A replay's borrow does not move it (`Settings_Borrowing`).
- Yes and No never write the per-fix settings, so Custom brings the player's choices back.
- Both kinds of row are recorded, so a recording says which rules it ran under. Every `fix_` row gives `SETTING_BEFORE(0)`, and the master is last in its table, so `settings_before fix_` is the original's rules; `scripts/ci/check-settings-before.py` holds a `fix_` row to giving a value.

Which issues get a fix here is decided per issue before work starts, into one of three buckets:[^plan]

- **Retail fix:** the original has it and its designers did not mean it. `fix_` setting, on, under the row.
- **Enhancement:** the game works as designed and lba2cc changes the design. Its own setting, off by default, not under the row.
- **Display option:** changes only what is seen or heard, never a replay. With the display settings.

A port bug (the original was right and the port broke it) and a port difference (lba2cc followed the 2022 release where the CD release differs) are neither: they are restored, with no setting.

# Non-goals

- **A switch per fix in the menu.** The menu has the one row; the per-fix settings are for the console and the cfg.
- **The whole of the original's rules.** The row covers retail fixes only. An enhancement that changes play has its own setting outside it, so a later "Original" preset or watermark has to check every setting that changes play, not this row alone.
- **Undoing what a fix wrote.** A fix that sets game state leaves it in the save; switching the fix off changes the rules from then on.
- **Removing a retail fix's switch.** Baking a fix in would make the original unreachable for that bug, which is a change to Principle 3 to make on its own, not a tidy-up.

What could reopen it: a fix whose original path cannot be kept alive next to the new one at reasonable cost, such as a large second code path (the outdoor crossing, #707), or a player-facing request for per-fix control in the menu.

[^issue]: Issue 760 on origin, the five questions and the shape agreed in its comments.
[^pr]: Pull request 759 on origin, `DoFoundObj` in INVENT.CPP and its fixture.
[^module]: [SOURCES/RETAILFIX.H](../../../SOURCES/RETAILFIX.H) and RETAILFIX.CPP's table.
[^plan]: [docs/plan/RETAIL_FIX_PLAN.md](../../plan/RETAIL_FIX_PLAN.md), "Which bucket an issue goes in".
