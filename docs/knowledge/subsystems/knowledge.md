---
type: Subsystem
title: Knowledge bundle
subsystem: knowledge
description: Design principles, contracts, and seams for the docs/knowledge OKF bundle.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T14:00:00Z }
constrains: /SCHEMA.md
---

The bundle is a subsystem like any other, so it gets a charter like any other.
This document holds the judgment calls. `SCHEMA.md` holds the mechanical rules
and the lint enforces them. If a statement here could be a lint rule, it is in
the wrong file.

Read this when deciding whether something should be a concept at all. Read
`SCHEMA.md` when you already know it should be and need to shape it.

# Principles

**The bundle holds what the source cannot tell you.** Graphify answers what
calls what. The compiler answers what the types are. Git answers who changed
it. If any of those can produce the answer, writing it here creates a second
copy that will be wrong within a release. What survives is the part with no
mechanical oracle: why a decision was taken, what the original binary
guarantees, which behaviour is load bearing and looks like a defect.

**One owner per fact, which does not mean one document.** Duplicated truth is
the failure mode that killed the previous docs layout, where charter, plan, and
RFC each asserted a different version of the same architecture. Exactly one
concept owns each fact and every other mention is a link.

The exception matters because the naive reading of this principle destroys
real knowledge. One mechanism can present as a proposed cause, then as
a hazard investigated and dismissed as real but not this one, then as the
actual blocker. Those are one fact in three sets of clothes, and merging them
into a single clean statement deletes the record of how the mechanism has been
misread, which is the part that prevents the next rediscovery. Own the
mechanism once, keep the presentations, link them with `manifests`.

The test is whether the second document adds a way the fact has been
encountered. If it does, keep it. If it only restates the fact, merge it.

**Evidence outranks assertion.** Every substantive claim carries what backs it.
A confident sentence with nothing behind it is worse than an explicit gap,
because a gap invites a research gate and a confident sentence invites a
refactor. This is why `equivalence` exists and why the low end of it must be
stated in the body rather than left to frontmatter.

**Record the traps, not just the guarantees.** A bundle that documents only
what each thing promises trains its readers to over-read every field. The
expensive mistakes come from the opposite direction: assuming an identifier
identifies more than it does, assuming a flag says when it applies, assuming an
absent trait is a false trait rather than one that did not exist yet. What a
concept does not tell you is content, and it is usually the content that was
expensive to learn. Two true statements about different configurations are not
a contradiction, and the way that mistake gets made is by deleting one of them.

**Absence is legible, never fatal.** An unverified concept is distinguishable
from a verified one and is still consumable. Nothing in this bundle may be
built to reject a concept for missing optional metadata. Half a page of true,
unverified context beats no page.

**Knowledge ships with the change that made it true.** A concept is updated in
the PR that changed the behaviour, not in a documentation sweep afterwards.
Curation is a normal engineering activity here: review, diffs, and blame all
work because this is plain markdown in git.

**Format over tooling.** OKF separates who writes knowledge from who reads it.
Nothing in the bundle may depend on a specific editor, plugin, or agent. If a
concept is only useful inside one tool, that tool has become the format.

# Contracts

What the bundle guarantees to anything reading it:

- Every non-reserved file is a conformant OKF v0.2 concept with a routable
  `type`. A consumer that does not recognise a `type` treats it as generic
  rather than failing.
- A concept ID is its bundle-relative path, `.md` included, and IDs are
  stable. Moving or retiring a concept goes through `supersedes` and a
  `deprecated` status, never a silent rename, because inbound links and agent
  memory both key on the path.
- Typed relations and body links resolve, and CI's link gate checks both. A
  not-yet-written concept is named in prose or given a `draft` stub, because
  a dangling link is indistinguishable from a typo.
- Generated concepts are reproducible from their declared inputs. Regenerating
  produces no diff when the inputs have not changed.
- No concept asserts a structural claim without naming its oracle, whether that
  is a Graphify query, a disassembly reference, or a test.
- Nothing in the bundle is required to build, run, or ship the engine.

# Seams

Where this subsystem touches everything else, and which direction data flows:

| Seam                       | Direction | Contract                                                                 |
| -------------------------- | --------- | ------------------------------------------------------------------------ |
| Graphify                   | read      | Structural oracle. Concepts cite the query text and never transcribe results. The graph is untracked, so a citation is re-derivable only by rebuilding it. |
| `ASM_VALIDATION_PROGRESS.md`, `ASM_TO_CPP_REFERENCE.md` | read | Porting-status oracles, per routine and per module. The bundle mirrors them and never contradicts them. |
| `scripts/dev/knowledge_porting.py` | write | The only writer of generated concepts. A script, because the reproducibility contract above cannot be met by a model. |
| `AGENTS.md`, `CONTRIBUTING.md` | read  | Editing standard. There is no ownership map; a concept is reviewed by whoever reviews the code it describes. |
| CI lint                    | read      | Enforces `SCHEMA.md`. `scripts/ci/check-knowledge.py`, in the docs-links workflow, blocking. |
| User-facing docs           | neither   | The bundle owns invariants about them, never their text.                  |

The Graphify and reference-file seams are read-only on purpose. The bundle is
downstream of ground truth, and a bundle that can write back upstream becomes a
second source of truth by accident.

# What it does not tell you

- Whether a concept is true. `verified` says a human looked and `equivalence`
  says what evidence backs it; neither is a proof, and a `stable` status is a
  lifecycle claim, not a correctness one.
- How the code works. A concept that could be replaced by reading the source
  or running a query has failed the first principle, so the absence of a
  concept about a mechanism is not evidence that nobody understands it.
- What changed since `as_of`. A concept about a subsystem under active change
  is current at that commit and silent about everything after it.

# Non-goals

- Not a code graph, a plan archive, or user-facing documentation.
- Not complete. Coverage is not a goal and a coverage metric would drive
  exactly the low-evidence filler this charter exists to prevent.
- Not a memory store for agent sessions. Durable project knowledge only. A
  session-derived finding enters through the log, re-derived against the tree
  first.
- Not an authoring surface for any specific editor.

# Open questions

- Charters are absorbed as `Subsystem` concepts; this document is one, which
  makes the bundle self-hosting, as `SCHEMA.md` already was. Whether a
  `docs/charters/` directory is ever wanted for anything else is open.
- Whether `Reference` is doing too much, since it currently covers external
  material, agent skills, and invariant anchors for user-facing docs.
- Whether `Quirk` now demands too much at authoring time. It requires
  `equivalence`, `asm_origin`, `scope`, and two body sections, which is heavy
  for the type most likely to be written in the moment something is discovered.
  The failure to watch for is not a badly scoped `Quirk` but no `Quirk` at all,
  with the knowledge staying in a PR comment. A `draft` tier enforcing only
  `scope` and a one-line negative-space statement, with the rest gated at
  promotion to `stable`, is the obvious mitigation if the recorder slice shows
  quirks arriving from retros rather than from the work. The first evidence
  points that way: both Quirks in the recorder slice came from a migration
  session rather than from the work, and the one quirk-shaped fact found
  mid-work during that slice went into a parked document instead of here.
