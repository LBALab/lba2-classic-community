---
type: Reference
title: Purpose tasks
description: Four refactors that have happened or nearly happened in this repository, each with the concept that should stop it, run as a two-arm manual test of whether an agent with the bundle leaves load-bearing code alone; the only test of what the bundle is for rather than whether it is well formed.
status: draft
resource: ../../../tests/knowledge/README.md
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T17:00:00Z }
relates_to:
  - /subsystems/knowledge.md
sources:
  - id: tasks
    resource: ../../../tests/knowledge/README.md
    title: The four tasks, how to run both arms, and how to score
---

# What it tests

The lint checks that the bundle is well formed. Nothing in CI checks that it works, and the charter's first purpose is that a reader about to change the engine leaves load-bearing behaviour alone. The four tasks are the test of that: a symmetric pair of fade guards, a save version bump for a cleaner reader, the removal of a pump call that looks like a no-op, and a video player that hands back the previous screen. Each is a change this repository has made or nearly made, so the right answer is known, and each has a concept whose job is to stop it.[^tasks]

# How it is run

Manually, two arms per task, the same agent and prompt with and without the bundle in the working copy, recording whether the agent opened a concept, whether it made the change, and what reason it gave. A pass is declining with the load-bearing reason. The confound is stated in the fixture: the source comments already carry part of this knowledge, so the arm without the bundle is not knowledge-free, and what the test measures is the bundle's value over the comments.[^tasks]

# What it does not tell you

- Whether the bundle is right. An agent can decline a change for a reason a concept states and the concept can still be wrong; the review of the concepts is a separate activity.
- Anything about concepts the four tasks do not reach. Four Quirks and two Decisions are exercised; the rest of the bundle is not, and a task is added when a slice lands a concept an agent would plausibly refactor away.

[^tasks]: [tests/knowledge/README.md](../../../tests/knowledge/README.md).
