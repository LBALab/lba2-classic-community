---
type: Decision
title: One flag table drives validation and two-tier help
description: Every flag the engine accepts is a row in one table with its arity, case rule, player-facing bit and section; validation walks the table before SDL starts and rejects what is not in it, --help prints the player subset and --help-all the grouped whole, and neither names a repository path.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T09:00:00Z }
constrains:
  - /subsystems/control.md
sources:
  - id: cli-args-h
    resource: ../../../SOURCES/CLI_ARGS.H
    title: CLI_ARGS.H, the table and the two entry points
  - id: control-doc
    resource: ../../CONTROL.md
    title: CLI control harness (docs/CONTROL.md), "Usage"
  - id: cli-test
    resource: ../../../tests/cli_args/test_cli_args.cpp
    title: The host test for accept and reject shapes and both tiers
---

# Context

A mistyped flag ran silently on defaults, so a run could look like an A/B and be two runs of the same thing. And a single `--help` listed every automation and self-test flag beside the handful a player needs to set up a run.[^control-doc]

# Decision

- **One table.** Each flag is a row: name, number of values, case sensitivity, whether a player reaches for it, which section it belongs to, its arguments and its help line. A flag handled anywhere without a row is rejected as unknown.[^cli-args-h]
- **Validation before SDL.** The table is walked by arity and three silent-wrong-answer shapes are rejected with a non-zero exit: an unknown flag, a missing value, and a flag standing where a value belongs. Handled in `main` next to `--version`, so no window and no game data are needed to fail.[^cli-args-h]
- **Two tiers.** `--help` prints only the rows marked for players, with a quick-start line. `--help-all` prints every row, grouped by section, generated from the same table so it cannot drift from what the parser accepts.[^control-doc]
- **No repository path in either tier.** A release ships no repository docs, so help that said "see docs/X.md" would point a player at a file they do not have. The help stands on its own.
- **The CLI points at the console for runtime verbs.** `--exec` and `--exec-at` are the bridge, and the console's own `help`, `cmdlist` and `listsaves` are where the verbs are documented.

# Non-goals

- **Documenting console verbs in CLI help.** Two front-ends, one engine; the CLI does not re-document what the console already explains.
- **A flag outside the table.** There is no second path for accepting an argument, and adding one defeats the rejection of typos.
- **Marking a flag player-facing to make it discoverable.** The common bit is for a flag a player setting up a run reaches for; automation flags stay in the full tier.

[^cli-args-h]: SOURCES/CLI_ARGS.H, `Cli_ValidateArgs` and `Cli_WantsHelp`.
[^control-doc]: docs/CONTROL.md, "Usage".
[^cli-test]: tests/cli_args/test_cli_args.cpp.
