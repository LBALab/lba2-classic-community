# Principles

The rules in this repository follow from a few principles. When a case comes up that no rule
covers, reason from these. Each has the reason it holds and what it means in practice; the rings,
the core and the platform tiers in [ARCHITECTURE.md](ARCHITECTURE.md#the-rings-and-the-core) are
built on them.

## 1. The original is the reference

*Why:* without a fixed reference, nobody can tell a fix from a drift.

The original is the game as the 1990s CD release played it, not the 2022 Classic release.

- With every setting at its value from before (`SETTING_BEFORE` in
  [SOURCES/SETTINGS.H](../SOURCES/SETTINGS.H)), the game plays as the original. A change that
  moves it away is a regression; a change that brings it back is a fix and needs no switch.
- A fix to a bug the original itself had gets its own switch, on by default, under **Fix
  retail bugs**, whose No gives back the original's rules
  ([the retail-fix Decision](knowledge/decisions/a-retail-fix-is-a-switch-under-one-row.md)).
  A change to the design is an enhancement: its own setting, off by default.
- When the source and the release disagree, the release wins, unless a Decision keeps the
  port's choice and says why.
- Where the original has nothing to say, the surface is ours: save names, touch, the name
  editor, a guard for an input no play can reach. It follows the other principles.

## 2. Understand before you change

*Why:* code that looks wrong is often load bearing.

- Read the owning Subsystem and its Quirks under [knowledge/](knowledge/index.md) before
  changing behaviour that looks wrong.
- Original behaviour that looks like a bug but is load bearing is kept and recorded: a Quirk,
  or for a small one a comment where it sits.
- A change to what a core contract promises (save and data formats, the replay digest, the
  recording header, what a settings row means, the PAL) starts from a Decision or amends one.
  A change that contradicts a Decision updates it in the same PR. Rewriting core code that
  keeps its contract, such as a save path that writes the same bytes, needs no Decision.

## 3. Every claim can be re-derived

*Why:* the understanding we record is what lasts, and only if someone else can check it.

- A claim in a doc, PR or commit names its source: a test, a recording, a file and line, an
  address in the release, a PR.
- A number comes with what remakes it: the command, or the probe patch attached to the PR.
  A number measured before the code changed is measured again.
- Say what was not checked. A finding that is withdrawn takes the claims built on it with it.

## 4. Measure to prove yourself wrong

*Why:* a measurement that could not have failed proves nothing.

- Every measurement has a control: main, the setting off, or the old code.
- Prove the check can fail before trusting it: a test, golden or probe can encode the bug it
  should catch, or pass without doing its work. A check that can pass silently is a bug.
- Measure on the data players use (US CD, GOG, Steam), not one fixture.
- Run the committed fixtures a change can reach before merging it.
- A fix to the core may replace recordings and goldens: remake them with a control first (the
  old code reproduces the old file), and narrow what cannot be remade. The core is not bent to
  keep an old file.

## 5. Leave room to play

*Why:* experiments are how the project finds what is worth keeping, and they bring people in.

- An Experimental setting is off by default, comes out in one PR, and needs a Decision only
  if it touches a core contract.
- Dependencies point inward. The core reaches an experiment only through a hook that does
  nothing when it is off, so taking the experiment out breaks nothing.

## 6. Quality keeps people; novelty does not

*Why:* writing a feature is cheap; owning and polishing it is not.

- Fixing and polishing come before new features.
- Anything judged by feel gets a play test, read back from its trace: a new setting and a fix
  to an existing one alike. A rule that runs each game step is checked at `--fixed-dt 2`.
- One topic per PR, one change per commit, each revertible on its own. A PR whose commits
  already separate its changes need not be split.
- A feature nobody maintains stays Experimental, or comes out.

## 7. Promise only what you test

*Why:* a promise we cannot check becomes a bug report we cannot reproduce.

- A platform is an OS on a CPU architecture: an Intel Mac and an Apple Silicon Mac are two
  platforms, and so are 32-bit and 64-bit Android.
- A platform's tier sets what it is promised
  ([ARCHITECTURE.md "Where it runs"](ARCHITECTURE.md#where-it-runs)).
- The README and release notes claim nothing beyond a platform's tier. A tier 3 build may ship
  in a pre-release if it is labelled experimental and untested.
- Docs promise nothing about what comes next; they point to milestones.
