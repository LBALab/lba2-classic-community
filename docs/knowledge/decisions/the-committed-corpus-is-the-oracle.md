---
type: Decision
title: The committed save corpus is the oracle for what retail wrote
description: Fifty retail saves live in the tree, anonymised in place with their byte lengths kept, and are the regression baseline for the load path and the ground truth for the wire layout; larger contributed sets stay out of the tree under an ignore rule, and the driver that runs any of them needs retail data, so it runs locally rather than in public CI.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
relates_to:
  - /formats/lba-save.md
  - /decisions/compressed-saves-round-trip-byte-for-byte.md
  - /references/save-tests.md
sources:
  - id: wire-plan
    resource: ../../plan/SAVE_WIRE_PLAN.md
    title: Save wire-format plan, decision 3
  - id: corpus-readme
    resource: ../../../tests/savegame/corpus/README.md
    title: The corpus harness README
  - id: saves-readme
    resource: ../../../tests/savegame/corpus/saves/steam_classic_2023/README.md
    title: The corpus's provenance, licence rationale and anonymisation
  - id: gitignore
    resource: ../../../tests/savegame/corpus/.gitignore
    title: The ignore rule with its one opt-in
  - id: agents
    resource: ../../../AGENTS.md
    title: The repository's rule on game assets
---

# Context

The wire-format campaign was briefed on the assumption that retail saves stay local and ignored, with a private corpus reaching CI through an environment variable. The tree already contradicted that: fifty saves from the Steam re-release were committed under `tests/savegame/corpus/saves/`, every one compressed and in the retail layout, with an ignore rule that excludes every corpus directory and opts that one and the two legacy fixtures back in, a written case that a save is runtime state referencing the game's archives rather than an asset of the game, and two player names replaced in place with strings of the same length so that every offset and the compressed size were unchanged.[^saves-readme][^gitignore]

# Decision

Treat the committed fifty as the oracle. They are the regression baseline for `SAVEGAME.CPP`, expected to load fifty for fifty under the automatic reader and under the forced wire reader, and they are the ground truth for what the retail engine wrote: the wire sizes and the 276-byte object stride were derived from the native structs and then confirmed against these files. A larger contributed set was validated the same way and stays under the ignore rule, run locally. The driver runs the engine, so it needs the retail archives; it is a make target and a retail-gated job, not a public CI step, and the plan's phrase for it, a CI oracle, is true only of the gated job.[^corpus-readme][^wire-plan]

The driver also checks the offline probe against the engine per save, which turns the probe into a falsifiable contract: a change to either that makes them disagree shows up as a divergence in the run.[^corpus-readme]

# Non-goals

- **Committing game assets.** The archives the saves reference are the maintainer's rule to never add, and a save is committed on the argument that it is not one; the argument is written beside the files and is what a future corpus has to meet.[^agents]
- **Committing further corpora.** The ignore rule's default is out, and each opt-in is one line for one directory, two at `as_of`.
- **Committing a file with a real name in it.** The anonymisation preserved byte length so the corpus stayed bit-identical elsewhere; a new file goes through the same step before it goes in.
- **A public CI run.** The host tests cover what runs without retail data; the corpus does not, and a green CI says nothing about it.

[^wire-plan]: docs/plan/SAVE_WIRE_PLAN.md, "Decisions (resolved 2026-07-07)", item 3, and "Decision 3: corpus provenance and handling".
[^corpus-readme]: tests/savegame/corpus/README.md, "Requires retail game data", "Bundled reference corpus" and the probe-versus-harness check.
[^saves-readme]: tests/savegame/corpus/saves/steam_classic_2023/README.md, "Provenance" and "Anonymization".
[^gitignore]: tests/savegame/corpus/.gitignore.
[^agents]: AGENTS.md, the rule against adding game assets.
