---
type: Reference
title: lba2cc knowledge bundle schema
description: House profile constraining OKF v0.2 for the lba2cc knowledge bundle.
status: draft
generated: { by: human:noctonca, at: 2026-09-09T00:00:00Z }
---

# Purpose

`docs/knowledge/` is an OKF v0.2 knowledge bundle: the internal, agent-readable
record of what is true about this engine and why. OKF fixes the container
(markdown plus YAML frontmatter, one concept per file, `type` required) and
explicitly declines to define a type taxonomy. This document is that taxonomy,
plus the relation vocabulary and lint rules that make the bundle checkable
rather than merely conventional.

Read this before authoring or generating any concept.

# Non-goals

- Not a code graph. Graphify already answers "what calls what" from the source.
  A concept that restates an EXTRACTED edge is duplicated truth and will rot.
  Link to the query, do not transcribe its result.
- Not a replacement for `ASM_TO_CPP_REFERENCE.md`. That file stays the single
  generated ground truth for porting status. `Porting Status` concepts are a
  projection of it, never an independent claim.
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
  subsystems/                 one per docs/charters/ entry
  decisions/                  ADR-shaped records
  formats/                    on-disk and wire formats
  quirks/                     load-bearing original-engine behaviour
  porting/                    GENERATED. projection of ASM_TO_CPP_REFERENCE.md
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
| `Subsystem`      | Design principles, contracts, seams for one subsystem        | `subsystem`, body `# Contracts`, `# Seams` |
| `Decision`       | One decision, its context, and what it forecloses            | body `# Decision`, `# Non-goals`          |
| `Format`         | An on-disk or wire format, field by field                    | `equivalence`, body `# Layout`            |
| `Quirk`          | Original-engine behaviour that must be preserved             | `equivalence`, `asm_origin`, body `# Why it is load bearing` |
| `Porting Status` | Per-subsystem porting state. GENERATED, never hand-edited    | `subsystem`, `sources`                    |
| `Reference`      | External material, run instructions, agent skills            | `resource`                                |

Six types is deliberate. Every additional type is a routing decision an agent
has to get right, and OKF consumers must tolerate unknown types gracefully,
which means a wrong type fails silently rather than loudly. Add a seventh only
when a concept genuinely cannot be expressed as one of these.

`Quirk` is the highest-value type in the bundle. It is the knowledge an agent
will confidently destroy during a refactor, because the code looks wrong and
the reason it is right lives nowhere in the source.

# Typed relations

OKF links are deliberately untyped: the spec conveys relationship kind through
surrounding prose. That is too weak for mechanical checking, so relations live
in frontmatter as producer-defined keys. Each value is a bundle-relative path
or a list of them.

| Key                | Meaning                                                       |
| ------------------ | ------------------------------------------------------------- |
| `ports`            | This concept describes the C++ counterpart of that ASM routine |
| `constrains`       | This concept limits what that concept may do                   |
| `supersedes`       | This concept replaces that one, which should be `deprecated`   |
| `verified_against` | The artifact this concept was checked against                  |
| `relates_to`       | Untyped association. Use sparingly, prefer a specific key      |

Body links remain normal markdown and remain untyped. Use the bundle-relative
absolute form (`/subsystems/save.md`), which the spec recommends because it
survives file moves within a subdirectory.

# Trust, equivalence, and lifecycle

Actors follow the OKF convention. This project uses three forms:

- `human:noctonca` for hand-authored or human-confirmed content
- `claude-code/<model>` for agent-generated content
- `process:wiki-writer` for the regeneration subagent

The `human:` prefix is load bearing: OKF derives trust tiers from it
(unverified, machine-confirmed, human-reviewed). An agent MUST NOT write a
`human:` actor.

```yaml
verified: [{ by: human:noctonca, at: 2026-09-09T00:00:00Z }]
equivalence: <status value from ASM_TO_CPP_REFERENCE.md, see below>
asm_origin: "SOURCES/AFF_OBJ.CPP:412"
stale_after: 2027-01-01T00:00:00Z
```

`equivalence` is this profile's one substantive addition to OKF, and it exists
because ASM-equivalence is the project's ground truth while `verified` only
records that a human looked. The two are independent: a human can review a
claim that was never checked against the disassembly, and a claim can be
disassembly-checked without anyone having reviewed the write-up.

The distinction it preserves is what evidence backs a statement, which is not
recoverable from `verified`:

- checked against the original disassembly
- covered by a passing equivalence or regression test, so behaviour matches
  even if the instruction sequence was never compared
- derived from reading the C++ only
- written down and checked against nothing

> OPEN: the enum values are not fixed here. Adopt whatever status vocabulary
> `ASM_TO_CPP_REFERENCE.md` already uses, verbatim, and map the third value
> onto Graphify's INFERRED tag rather than defining a parallel scale. Three
> overlapping vocabularies for the same axis is worse than one imperfect one.
> Extend the reference file only if it cannot express a whole-format state.

The lowest two levels are a research gate, not a fact, and a `Quirk` or
`Format` concept sitting at either MUST say so in the body. The failure mode
this prevents is expensive in one direction only: an agent that reads a
hypothesis as a constraint leaves working code alone, while an agent that reads
a constraint as a hypothesis refactors away behaviour the engine depends on.

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
> GENERATED by process:wiki-writer. Do not hand edit. Regenerate from
> ASM_TO_CPP_REFERENCE.md and Graphify EXTRACTED edges.
```

Regeneration triggers: the owning charter changes, the concept's
`ASM_TO_CPP_REFERENCE.md` rows change, or a milestone push. A hand edit to a
generated concept is a lint failure, not a merge conflict to resolve.

Every other type is hand-authored or agent-drafted then human-reviewed. The
`wiki-writer` subagent is scoped to synthesis only: no design authority, must
distinguish EXTRACTED from INFERRED when making structural claims, and must
flag a contradiction between charter and code rather than silently resolving
it.

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
4. A relation key whose target does not resolve in the bundle. Broken body
   links are tolerated per OKF, since they may represent not-yet-written
   knowledge. Broken typed relations are not.
5. A concept past `stale_after` with `status: stable`.
6. A `human:` actor written in a commit authored by an agent.
7. Obsidian-only syntax: `[[wikilinks]]`, Dataview blocks.
8. A hand edit to a `Porting Status` concept.
9. `supersedes` pointing at a concept whose `status` is not `deprecated`.
10. A concept carrying `asm_origin` whose `equivalence` disagrees with that
    routine's status in `ASM_TO_CPP_REFERENCE.md`.

Rules 4, 5, 8, 9, and 10 are the drift checks. They are the reason this profile
exists rather than a README of conventions.

Implement these after the first slice, not before it. Rules written against
concepts that do not exist yet tend to address problems the bundle does not
have.

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

Two slices before bulk migration, chosen for different purposes.

**Breadth: the recorder/observer work.** It crosses input, determinism,
serialization, and the test harness, which is where a type catalogue either
holds or visibly fails. Run the retro process against it first and use the
retro output as the distillation input: a retro on merged multi-subsystem work
produces exactly the durable residue this migration wants, already extracted.

**Depth: one ASM-grounded concept.** The recorder is new construction, so it
exercises none of `equivalence`, `asm_origin`, `ports`, or `Porting Status`.
Those are the most novel parts of this profile and the most likely to be wrong.
One routine, or the v37 format, is enough to smoke-test the porting axis.
