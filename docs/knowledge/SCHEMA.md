---
type: Reference
title: lba2cc knowledge bundle schema
description: House profile constraining OKF v0.2 for the lba2cc knowledge bundle.
status: draft
resource: https://github.com/GoogleCloudPlatform/knowledge-catalog/tree/main/okf
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T14:00:00Z }
relates_to: /subsystems/knowledge.md
---

# Purpose

`docs/knowledge/` is an OKF v0.2 knowledge bundle: the internal, agent-readable
record of what is true about this engine and why. OKF fixes the container
(markdown plus YAML frontmatter, one concept per file, `type` required) and
explicitly declines to define a type taxonomy. This document is that taxonomy,
plus the relation vocabulary and lint rules that make the bundle checkable
rather than merely conventional.

The judgment calls, and whether something should be a concept at all, live in
the bundle's charter, `/subsystems/knowledge.md`. This file holds the
mechanical rules. If a statement here cannot become a lint rule, it is in the
wrong file.

Read this before authoring or generating any concept.

# Non-goals

- Not a code graph. Graphify already answers "what calls what" from the source.
  A concept that restates an EXTRACTED edge is duplicated truth and will rot.
  Link to the query, do not transcribe its result.
- Not a replacement for `ASM_VALIDATION_PROGRESS.md` or
  `ASM_TO_CPP_REFERENCE.md`. Those stay the ground truth for porting status,
  the first per routine and the second per module. `Porting Status` concepts
  are a projection of them, never an independent claim.
- Not user-facing documentation. See "Audience boundary" below.
- Not an authoring surface for Obsidian. Obsidian may open the directory
  read-only for graph browsing. Wikilinks, Dataview, and other Obsidian-only
  syntax are forbidden, since they break agent and CI consumption.
- Not a plan archive. Plans describe intent at a moment. They enter the bundle
  only once their durable residue has been extracted.

# Bundle layout

```
docs/knowledge/
  index.md                    okf_version: "0.2" at bundle root only
  log.md                      date-grouped, newest first
  SCHEMA.md                   this file
  subsystems/                 one per subsystem: the charters, this bundle's included
  decisions/                  ADR-shaped records
  formats/                    on-disk and wire formats
  quirks/                     load-bearing original-engine behaviour
  porting/                    GENERATED. projection of ASM_VALIDATION_PROGRESS.md
  references/                 external material, skills, attesters
```

Directory placement carries no semantics beyond grouping. `type` is what
consumers route on. `index.md` and `log.md` are reserved at every level and are
never concept documents.

# Frontmatter: required of every concept

```yaml
type: <one of the type catalogue below>   # REQUIRED by OKF
title: <display name>                     # REQUIRED by this profile
description: <one sentence>               # REQUIRED by this profile
status: draft | stable | deprecated       # absent means stable
generated: { by: <actor>, at: <ISO 8601 UTC> }
```

OKF requires only `type`. This profile additionally requires `title`,
`description`, and `generated`, because `index.md` generation and agent
triage both depend on them. Everything else is per-type.

`generated.at` records the last meaningful content change, not the last commit
touching the file.

# Type catalogue

| `type`           | Owns                                                        | Additionally required                     |
| ---------------- | ----------------------------------------------------------- | ----------------------------------------- |
| `Subsystem`      | Design principles, contracts, seams for one subsystem        | `subsystem`, body `# Contracts`, `# Seams`, `# What it does not tell you` |
| `Decision`       | One decision, its context, and what it forecloses            | body `# Decision`, `# Non-goals`          |
| `Format`         | An on-disk or wire format: its guarantees, its versioning rule, and what it does not say. Field by field only where nothing in the tree already is | `equivalence`, body `# Layout`, `# What it does not tell you` |
| `Quirk`          | Original-engine behaviour that must be preserved             | `equivalence`, `asm_origin`, `scope`, body `# Why it is load bearing`, `# What it does not tell you` |
| `Porting Status` | Per-subsystem porting state. GENERATED, never hand-edited    | `subsystem`, `sources`, `stale_after`     |
| `Reference`      | External material, run instructions, agent skills            | `resource`                                |

Six types is deliberate. Every additional type is a routing decision an agent
has to get right, and OKF consumers must tolerate unknown types gracefully,
which means a wrong type fails silently rather than loudly. Add a seventh only
when a concept genuinely cannot be expressed as one of these.

`Quirk` is the highest-value type in the bundle. It is the knowledge an agent
will confidently destroy during a refactor, because the code looks wrong and
the reason it is right lives nowhere in the source.

`scope` names the configuration a Quirk holds under: loose clock against
pinned, `--load` against fresh boot, headless against windowed. This engine
forks behaviour on all three, and an unscoped quirk reads as universal until a
reader finds the comment about the other configuration and resolves the
apparent contradiction by deleting one side.

`# What it does not tell you` lists what a reader would over-read into the
concept's subject: a field that identifies less than it seems, a flag that says
that but not when, an absence that meant nothing before the trait existed. Two
true statements about different configurations are not a contradiction.

# Typed relations

OKF links are deliberately untyped: the spec conveys relationship kind through
surrounding prose. That is too weak for mechanical checking, so relations live
in frontmatter as producer-defined keys. Each value is a bundle-relative path,
`.md` included, or a list of them; that path is also the concept's ID.

| Key                | Meaning                                                       |
| ------------------ | ------------------------------------------------------------- |
| `ports`            | This concept describes the C++ counterpart of that ASM routine |
| `constrains`       | This concept limits what that concept may do                   |
| `supersedes`       | This concept replaces that one, which should be `deprecated`   |
| `verified_against` | The artifact this concept was checked against                  |
| `manifests`        | This concept is one presentation of a mechanism that concept owns. The owner keeps the mechanism; the presentations keep how it was met |
| `relates_to`       | Untyped association. Use sparingly, prefer a specific key      |

An ASM routine is not a concept, so `ports` points at the routine's section in
its `Porting Status` concept (`/porting/3d.md#mulmatrixf`), and
`verified_against` points at a `Reference` that wraps the attesting artifact.

Body links remain normal markdown and remain untyped. Use the bundle-relative
absolute form (`/subsystems/save.md`), which the spec recommends because it
survives file moves within a subdirectory. `lychee.toml` resolves that form
against the bundle, so body links are checked by the same gate as every other
doc. Name a not-yet-written concept in prose, or give it a `draft` stub,
rather than linking to nothing: a dangling link is indistinguishable from a
typo.

# Trust, equivalence, and lifecycle

Actors follow the OKF convention. This project uses three forms:

- `human:noctonca` for hand-authored or human-confirmed content
- `claude-code/<model>` for agent-generated content
- `process:<generator>` for a generated concept; today that is
  `process:knowledge-porting`, which is `scripts/dev/knowledge_porting.py`

The `human:` prefix is load bearing: OKF derives trust tiers from it
(unverified, machine-confirmed, human-reviewed). An agent MUST NOT write a
`human:` actor.

```yaml
verified: [{ by: human:noctonca, at: 2026-09-09T00:00:00Z }]
equivalence: tested | partial | untested
asm_origin: "LIB386/3D/MULMATF.ASM:MulMatrixF"
scope: "loose clock, --load path"
as_of: cc01c7ae
stale_after: 2027-01-01T00:00:00Z
```

`asm_origin` is a file and a routine, never a line number: line numbers rot
on the next edit and nothing checks them, while a routine name is greppable
and is what a reader needs to land on the act. It names where in the original
source the behaviour lives: the `.ASM` for a LIB386 routine that was ported,
and the original C++ for SOURCES and for the parts of LIB386 that Adeline
wrote in C++, such as the timer. The key is one key because the question is
one question: what did Adeline ship.

`equivalence` is this profile's one substantive addition to OKF, and it exists
because ASM-equivalence is the project's ground truth while `verified` only
records that a human looked. The two are independent: a human can review a
claim that was never checked against the disassembly, and a claim can be
disassembly-checked without anyone having reviewed the write-up.

Its values are the legend of `ASM_VALIDATION_PROGRESS.md`, verbatim, because
that is the only status vocabulary the tree has: `ASM_TO_CPP_REFERENCE.md`
records per module what is built as C++ and carries no per-routine status.
The four kinds of evidence map onto three words unevenly, and the body carries
what the word cannot:

| Evidence                                                   | `equivalence` | The body must say                                   |
| ---------------------------------------------------------- | ------------- | --------------------------------------------------- |
| covered by a passing equivalence or regression test        | `tested`      | which test                                          |
| part of the routine's domain is under test                 | `partial`     | which part                                          |
| checked against the original disassembly, with no test     | `untested`    | that the comparison was made, and where it is recorded |
| derived from reading the C++ only                          | `untested`    | that it was read, not measured. Graphify's INFERRED edges are this level |
| written down and checked against nothing                   | `untested`    | that nothing backs it                               |

The first concept that needs a word for the third row extends the legend in
`ASM_VALIDATION_PROGRESS.md`; this profile does not define a parallel scale.

The lowest two rows are a research gate, not a fact, and a `Quirk` or
`Format` concept at `untested` MUST say in the body which row it sits on. The
failure mode this prevents is expensive in one direction only: an agent that
reads a hypothesis as a constraint leaves working code alone, while an agent
that reads a constraint as a hypothesis refactors away behaviour the engine
depends on.

`as_of` names the commit a concept's current claims were read from. Use it
while the subsystem is under active change and the concept is not yet
maintained in the PRs that change it. The charter's rule is that knowledge
ships with the change, so `as_of` is transitional and comes off once that
holds for the subsystem.

`stale_after` is required on `Porting Status` concepts and on any concept whose
truth is pinned to an unlanded PR.

# Body conventions

Favour structural markdown (headings, tables, fenced blocks) over prose. The
OKF spec makes this point and it matters more here than in most bundles,
because agents retrieve by section.

Per-claim attribution uses markdown footnotes keyed to a `sources[].id`, not a
citations list. Labels are keyed rather than positional so they survive an
agent reordering the list.

`# Non-goals` on a `Decision` is required, not optional. It is the single most
valuable convention already in use across the `*_PLAN.md` files and it is the
part an agent most reliably ignores when it is missing.

No em-dashes, per project convention.

# Generated versus hand-authored

`Porting Status` concepts are fully generated artifacts. They carry a banner as
the first body line:

```
> GENERATED by process:<generator> (<script>). Do not hand edit.
> Regenerate with `<command>`.
```

Regeneration triggers: the owning charter changes, the concept's
`ASM_VALIDATION_PROGRESS.md` rows change, or a milestone push. Regenerating
with unchanged inputs produces no diff, and the generator's `--check` says
whether a file has drifted. A hand edit to a generated concept is a lint
failure, not a merge conflict to resolve.

Every other type is hand-authored or agent-drafted then human-reviewed. An
agent drafting a concept is scoped to synthesis only: no design authority,
must distinguish EXTRACTED from INFERRED when making structural claims, and
must flag a contradiction between charter and code rather than silently
resolving it.

# Audience boundary

User-facing docs stay out of the bundle: `README.md`, `CHANGELOG.md`, build and
install instructions, controls, and any future modding guide. Different
audience, different lifecycle, and OKF's trust fields are meaningless on a doc
whose correctness is judged by a player rather than by the disassembly.

The bundle owns the invariants those docs must satisfy, not their text. The
README feature list is the working example: the list is user-facing, the rule
that it reflects shipped engine capability and excludes unlanded work is a
`Decision` concept. An agent editing the README consults the concept.

A `Reference` concept MAY point at a user-facing doc via `resource` when an
invariant needs an anchor. It MUST NOT duplicate its content.

# Lint rules

CI fails on:

1. A non-reserved `.md` without parseable frontmatter or without `type`.
2. A `type` outside the catalogue.
3. Missing `title`, `description`, or `generated`.
4. A relation key or a body link whose target does not resolve in the bundle.
   A not-yet-written concept is named in prose or given a `draft` stub.
5. A concept past `stale_after` with `status: stable`.
6. A `human:` actor written in a commit authored by an agent.
7. Obsidian-only syntax outside a code span: wikilinks, Dataview blocks.
8. A hand edit to a `Porting Status` concept.
9. `supersedes` pointing at a concept whose `status` is not `deprecated`.
10. A concept carrying `asm_origin` whose `equivalence` disagrees with that
    routine's row in its `Porting Status` concept.
11. A concept missing a key or body section its type requires, per the
    catalogue's third column.

Rules 4, 5, 8, 9, and 10 are the drift checks. They are the reason this profile
exists rather than a README of conventions.

The first slice has landed. The lint is advisory until it runs in CI, and
blocking after.

# Migrating existing docs

Sort every current doc into exactly one of four outcomes. Duplicated truth is
the failure mode to avoid: exactly one concept owns each fact, everything else
links to it.

| Outcome    | Applies to                                                    |
| ---------- | ------------------------------------------------------------- |
| Becomes    | Charters become `Subsystem`. Format specs become `Format`.     |
| Distils    | Merged plans and reviews. Extract the durable decision and the discovered quirks into concepts, then archive the plan outside the bundle. |
| Stays out  | User-facing docs, asset and content files, ephemeral notes.    |
| Dies       | Superseded docs with no residue worth keeping.                 |

Two slices before bulk migration, chosen for different purposes. Both landed
on 2026-09-09, and `log.md` records what they found.

**Breadth: the recorder/observer work.** It crosses input, determinism,
serialization, and the test harness, which is where a type catalogue either
holds or visibly fails. Run the retro process against it first and use the
retro output as the distillation input: a retro on merged multi-subsystem work
produces exactly the durable residue this migration wants, already extracted.

**Depth: one ASM-grounded concept.** The recorder is new construction, so it
exercises none of `equivalence`, `asm_origin`, `ports`, or `Porting Status`.
Those are the most novel parts of this profile and the most likely to be wrong.
One routine, or the v37 format, is enough to smoke-test the porting axis.
