---
type: Decision
title: The replay verdict is withheld from a run that did not cover the file
description: The success string a caller greps is printed only when the run checked something and checked at least half of what the file holds; the rule is that the verdict may not claim success while a trust condition it knows about is unmet, and the mode-lines predicate is deliberately not one of those conditions yet.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T16:00:00Z }
owner: /subsystems/recording.md
relates_to:
  - /formats/rec.md
  - /decisions/console-output-is-a-parsed-contract.md
  - /quirks/two-exterior-cubes-change-without-a-fade.md
sources:
  - id: record-cpp
    resource: ../../../SOURCES/RECORD.CPP
    title: RECORD.CPP, replay_covered_the_file, its comment, and the T_ReplayEnd enum
  - id: pr
    resource: https://github.com/LBALab/lba2-classic-community/pull/659
    title: fix(record), withhold the replay verdict from a run that did not cover the file
  - id: replay-test
    resource: ../../../tests/automation/test_record_replay.sh
    title: The arm that asserts a short replay withholds the success string
---

# Context

`[rec] replay ended ... first hash mismatch -1` is what a caller greps to mean the recording reproduced. A replay that stopped short can carry the -1 honestly, it matched every tick it reached, while not having replayed the session, and it printed the verdict anyway. Two instances, neither exotic. A recording replayed on a busy machine desynced inside a video and reported that it reproduced, over 51 of the 877 ticks the file holds, and which way that build failed depended on machine load. A recording cut inside its start snapshot had the snapshot refused and then printed the success string over zero ticks checked, which is deterministic and is in the suite. The stall and the menu had already been given their own outcomes for the same reason; a stream that ran out early was the third way and was not covered. The denominator existed and was thrown away: the extent scan printed `holds about N ticks` and returned nothing.[^pr]

# Decision

Two limbs, one rule: do not print the success string without evidence that the session was replayed. Nothing checked is never a verdict, whatever the extent says. Otherwise the run has to have checked at least half of the ticks the file holds. The extent is a structural undercount, the tick on the last sync marker in the trailing 64 KB, so a healthy replay always checks more than the file holds; over seven clean runs across two formats the ratio runs 1.006 to 1.472, so 1.0 is a floor for healthy runs and half is a wide margin below a floor, not a tolerance around a mean. It clears the tightest healthy run by two times and the one observed real failure by 8.6 times. Deliberately loose: too tight turns green fixtures red and gets the check reverted. A missing extent with ticks checked is not judged, because inventing a failure from a denominator that was never read is the same error in the other direction. The outcome is an enum with no default, so a new way out of a replay has to say which it is, and the short-stream outcome is named for coverage rather than termination, since a healthy replay also ends by the stream running out.[^record-cpp]

# What it does not catch

- A replay that covers 60% of the file still prints the success string. The band between half the file and all of it is unguarded; this is a first cut.
- A mode difference. `mode differs` is announced with the boot banner and ignored at the verdict, the same shape, and it is a different predicate on different evidence: whether the run was comparable to the one recorded, not whether it covered the file. It wants its own calibration and is out of scope by name. [The `.rec` Format](/formats/rec.md) and [the exterior Quirk](/quirks/two-exterior-cubes-change-without-a-fade.md) carry the consequence.[^pr]
- The console's `screenshot` verb printing its path before the capture completes is the same rule broken on the console's side, open as #669 under [the parsed-output Decision](/decisions/console-output-is-a-parsed-contract.md).

# Non-goals

- **A tolerance around the healthy ratio.** The threshold is a floor with a margin, chosen so that green fixtures stay green; tightening it is a new measurement, not a parameter change.
- **Judging a run with no extent.** The scan can come back empty on a file this has no quarrel with.
- **The console's running-replay surface.** It reports on a replay in progress and never on a finished one, so there is nothing there to withhold.
- **The mode predicate.** Out of scope by name, above.

[^record-cpp]: SOURCES/RECORD.CPP, `replay_covered_the_file` and the comment above it; the `T_ReplayEnd` enum and the comment above it.
[^pr]: Pull request 659 on origin, merged as 2727ea12: "Two instances, neither exotic", "The threshold is measured, not chosen", "What this does not catch" and "Not in here".
[^replay-test]: tests/automation/test_record_replay.sh, the arm that asserts a short replay withholds the success string while a full-coverage control over the same recording gets its verdict.
