# Architecture rules

A small set of boundaries the project already states in prose, checked mechanically on every
push. Companion to [CODESTYLE.md](../../CODESTYLE.md), which says what the result should look
like, and [REFACTOR_ROADMAP.md](REFACTOR_ROADMAP.md), which says which areas are worth
restructuring and in what order.

Measured against `main` on 2026-08-18. Every number below has the command that produced it;
re-run those rather than trusting the figures.

## What this is for

Two of the rules in CODESTYLE and AGENTS.md are load-bearing for work in flight and cost
nothing to check. The ownership campaign in the roadmap moves declarations out of
[C_EXTERN.H](../../SOURCES/C_EXTERN.H) one module at a time, and every module that lands makes
the god header smaller. Nothing stops the next feature putting one back, and nobody would
notice for months. That is the shape of every rule here: an invariant somebody already argued
for, with no mechanism behind it.

The value is not tidiness. It is that a boundary nobody checks decays silently, and the decay
surfaces years later as a coupling that has to be paid for at interest.

## What a rule has to earn

Three tests, and a candidate has to pass all three.

**It is already stated.** The rule comes from CODESTYLE, AGENTS.md or the seam doc. This file
adds enforcement, never a new opinion. A rule invented here would be one more person's taste
arriving as a build failure.

**It is checkable without understanding C++.** Include lists, preprocessor lines, counted
declarations, file membership in a CMake target. Anything that needs to know what a symbol
means needs a real parse, and a parse is a tool to maintain rather than a rule to follow.

**It is at zero today, or it only moves one way.** A gate starts clean and stays clean. A
ratchet starts wherever the tree is and is allowed to fall, never rise. A rule that lands red
teaches contributors to skip it, so anything with a real violation gets its violation fixed in
the same PR that introduces the rule.

## The rules

| # | Rule | Stated in | Today | Kind |
|---|---|---|---|---|
| 1 | `LIB386/` must not include `SOURCES/` | AGENTS.md | 0 | gate |
| 2 | No STL and no `std::` in shipped code | CODESTYLE | 0 | gate |
| 3 | Platform `#ifdef`s only in the platform layer | AGENTS.md, PLATFORM.md | 1 named exception | gate |
| 4 | `C_EXTERN.H` extern count may only fall | REFACTOR_ROADMAP | 252 | ratchet |
| 5 | `DEFINES.H` must not aggregate a new module header | CODESTYLE | 42 | ratchet |
| 6 | The `console` library must not depend on game code | CODESTYLE | 1 | gate, after a fix |
| 7 | No repo reference in a user-facing string | AGENTS.md | 2 | gate, after a fix |

### 1. LIB386 must not include SOURCES

The engine kernel is the half of the tree that a second title could reuse, and an include of
game code is the one edit that ends that. AGENTS.md states it as a bare requirement in the
Android row of "When modifying X, do Y".

```bash
grep -rn '#include' LIB386 --include=*.CPP --include=*.H | grep -c 'SOURCES/'   # 0
```

The reverse direction is already enforced by the build: `SOURCES/` reaches LIB386 only through
`<...>` angle includes resolved against `LIB386/H`, so the public header tree is the whole
contract and CMake's include paths keep it that way. No rule needed for that side.

### 2. No STL and no `std::` in shipped code

CODESTYLE: "No STL in shipped or per-frame code." The engine is C++98 with no STL, no
templates and no class hierarchy, deliberately. Tests are exempt and say so in the same
paragraph.

```bash
grep -rn 'std::' SOURCES LIB386 --include=*.CPP --include=*.H | grep -v libsmacker | wc -l   # 0
```

Exempt: `tests/`, `LIB386/libsmacker/`, vendored `stb_*`.

Reading that list from [.clang-format-ignore](../../.clang-format-ignore) looks right and is
not: that file also carries live engine sources whose hand-aligned tables the formatter would
wreck (`EXTRA.CPP`, `COMMON.H`, `MAPTOOLS.CPP`), and exempting those from an architecture rule
would leave a hole in it. A shared list is only worth having when both users want the same
thing, so the checker names the vendored trees itself.

### 3. Platform ifdefs only in the platform layer

AGENTS.md, on Android work: "keep callers `#ifdef`-free". [PLATFORM.md](../PLATFORM.md) §8
generalises it. The seam is a C-linkage entry point with a stub on hosts that lack the
feature, so a caller never learns which platform it is on.

The tree is already in that shape. Five files under `SOURCES/` carry a platform conditional,
and the [seam doc](../ENGINE_GAME_SEAM.md) labels all five platform:

| File | Conditional |
|---|---|
| [CLI_ARGS.CPP](../../SOURCES/CLI_ARGS.CPP) | `_WIN32` |
| [CONTROL_SERVER.CPP](../../SOURCES/CONTROL_SERVER.CPP) | `_WIN32` |
| [EXIT_SCREEN.CPP](../../SOURCES/EXIT_SCREEN.CPP) | `_WIN32` |
| [SCAN.CPP](../../SOURCES/SCAN.CPP) | `_WIN32`, MSVC vs GCC intrinsics |
| [TOUCH_INPUT.CPP](../../SOURCES/TOUCH_INPUT.CPP) | `__ANDROID__` |

Under `LIB386/` every hit is in `SYSTEM/`, `SVGA/`, `AIL/` or `H/SYSTEM/`, which is the
platform layer by definition. `<jni.h>` appears in exactly one file,
[ANDROID.CPP](../../LIB386/SYSTEM/ANDROID.CPP), which is the same rule at its narrowest.

The allowlist is the specification: a file joining it should be a line in a diff somebody
reads, not a silent `#ifdef`.

**One engine file carries them anyway.** [INITADEL.C](../../SOURCES/INITADEL.C) is the tree's
only `.C` file, which is why a `*.CPP`/`*.H` sweep does not see it. It holds two conditionals
of different kinds. The boot banner compiles in the host's name, which is a label rather than
behaviour and has nowhere else to live. The disc-image line picks a path separator by host,
which is real coupling, and it is redundant besides: `DiscImage_ComparePaths` in the platform
layer checks both separators unconditionally a few lines from the function that hands
INITADEL the path.

The file is therefore listed as a **named exception**, kept in its own constant rather than
folded into the platform allowlist. The two lists say different things: one says conditionals
belong here, the other says this is engine code that has not been cleaned up. The rule is only
as strong as the second list is short, and the basename belongs with the module that already
owns the path.

The block-comment wrinkle resolved itself. [LROT3D.CPP](../../LIB386/3D/LROT3D.CPP) and
[SCREEN.CPP](../../LIB386/SVGA/SCREEN.CPP) keep the original inline assembly inside `/* */`,
`#ifdef __MSC_VER` arms and all, so the checker blanks comments before reading any rule. That
costs about forty lines of state machine and buys the whole family: prose mentioning `std::`,
a commented-out include, and dead reference assembly all stop being violations.

### 4. The god header only shrinks

[REFACTOR_ROADMAP.md](REFACTOR_ROADMAP.md) uses one figure to describe the problem, and PR #563
used the same figure to describe its result: the extern count in
[C_EXTERN.H](../../SOURCES/C_EXTERN.H), 266 before the audio move and 252 after.

```bash
grep -cE '^\s*extern ' SOURCES/C_EXTERN.H   # 252
```

Adopting the maintainer's own metric matters more than the metric being perfect. It is already
the number quoted in commit messages, so the ratchet reports progress in the units the project
already thinks in, and an ownership PR lowers a constant in the same diff that lowers the
header.

State that is genuinely shared is not a violation. CODESTYLE's "state that is genuinely shared
stays shared" leaves five audio globals on the bus on purpose. A ratchet permits that: it
never demands a decrease, it only refuses an increase.

### 5. DEFINES.H must not aggregate a new module header

This is the rule that makes rule 4 worth having, and the roadmap is blunt about why. Ownership
has two halves: the header declares what the `.CPP` defines, and a caller reaching for that
state has to include the header, so the dependency becomes visible and everyone else loses the
ability to touch it by accident. The second half is unavailable while
[DEFINES.H](../../SOURCES/DEFINES.H) puts the module header in front of every translation unit
anyway.

DEFINES.H holds 65 includes, of which 42 are headers with a matching `SOURCES/*.CPP`, which is
the figure CODESTYLE quotes. One assertion covers both directions: the *set* of those 42 may
only shrink. A name appearing that is not in it means a module was aggregated again, and a name
missing from it means a move landed and the constant in the checker is stale. `AMBIANCE` left
in `3f762f37`; re-adding it would revert the expensive half of that work while leaving the
cheap half looking correct.

Two traps in counting this, both of which produce a plausible wrong number:

- **The includes are tab-separated.** A `grep -c '#include *"'` matches the space-separated
  ones only and quietly under-reports. Match `\s`, not a literal space.
- **`OBJECT.H` exists twice over.** DEFINES.H reaches the engine through `<OBJECT.H>` and the
  game's own module through `"OBJECT.H"`. Keying on the basename counts the engine header as
  an aggregated game module. Only the quoted form is an aggregation.

Comments matter here too: `INPUT.H`'s include is commented out, so it is not aggregated and
the rule says nothing about it.

### 6. The console library must not depend on game code

CODESTYLE describes this boundary as the mature form of the feature/surface split, and states
it as already true: "its core builds as a library that touches no game state, while
`CONSOLE_CMD.CPP`, which binds console verbs to game globals, is deliberately left out of that
library and compiled with the game."

It is one include away from true. The library is
[CONSOLE/CMakeLists.txt](../../SOURCES/CONSOLE/CMakeLists.txt)'s three files;
`CONSOLE_STATE.CPP` and `CONSOLE_GIVE.CPP` include no local header at all, and
[CONSOLE.CPP](../../SOURCES/CONSOLE/CONSOLE.CPP) includes `CHEATCOD.H` for one call to
`TryExecuteCheatByName`. The symbol is not in the library; it resolves when the game links,
which is why nothing has ever complained.

The fix is the pattern the same document prescribes: the feature registers with the surface.
A `Console_SetUnknownCommandHandler`, filled in by the cheat module, inverts the dependency and
leaves the library self-contained.

**The linker does not finish the job, and the symbol table is how you find that out.** The hope
was that a self-contained library would make the boundary the linker's business rather than a
script's. `nm -u` on the built archive says otherwise: with the cheat include gone, the console
core still leaves `Console_RegisterAll` undefined, because the core calls the game's
registration function through `Console_EnsureRegistered`.

That edge is the intended one and the difference is the whole point. `TryExecuteCheatByName`
arrived through `CHEATCOD.H`, a game module's header, so the library had the game's vocabulary
in scope. `Console_RegisterAll` is declared in `CONSOLE.H`, the library's own header: the
library states the contract and the game implements it. A static archive links either way, so
neither shows up as a build failure.

So the checkable rule is the include, not the link: no file in the `console` target may include
a header from outside the console module. The file list comes from the CMake target rather than
from a list in the checker, so a fourth file joining the library is covered the day it joins.

### 7. No repo reference in a user-facing string

AGENTS.md, in the Never list: "Never point a user-facing string at the repo. Help text, console
command descriptions, log lines, on-screen messages and dialogs reach people who have only the
binary: no `docs/*.md`, no source paths, no issue numbers."

Two live cases, both console command descriptions:

- [CONSOLE_CMD.CPP:2387](../../SOURCES/CONSOLE/CONSOLE_CMD.CPP#L2387), "See docs/RUNTIME_RESOLUTION.md."
- [CONSOLE_CMD.CPP:2468](../../SOURCES/CONSOLE/CONSOLE_CMD.CPP#L2468), "See docs/PERFTRACE.md."

Both should say the thing itself or point at `README.txt`, which ships.

The check reads string literals only, so a comment naming a doc stays legal, which it should:
`FOLLOWCAM_CFG.H` cites CAMERA.md in a comment and that is the right place for it. Issue
numbers need the pattern anchored tightly enough to leave
[EXIT_SCREEN.CPP](../../SOURCES/EXIT_SCREEN.CPP)'s "RULE #1024-GH" joke alone.

## What is deliberately not a rule

Four candidates that failed one of the three tests. Recording them matters as much as the list
above, because each will look attractive again later.

**A surface must not name a feature's variables.** Load-bearing, and the reason
[CONFIG_FILE.CPP](../../SOURCES/CONFIG_FILE.CPP) does not know a camera key name. It is not
visible in an include list: that file includes `FOLLOWCAM.H` precisely in order to call
`FollowCam_ReadConfig`, which is the rule being obeyed rather than broken. Telling the two
apart needs symbol-level analysis over `compile_commands.json`. Rule 6 is the subset that a
link boundary can express, and it is the part with teeth.

**Prefer `Log_*` over `printf`.** 118 call sites, 48 of them `fprintf(stderr`. A large share
are correct: AGENTS.md documents stdout as a pure data channel for `--dump-state` and the
`--exec` mirror, so `CLI_ARGS`, `CONTROL` and the recording tools are supposed to write there.
Separating those from a committed debug line needs a per-file allowlist longer than the rule.

**Fixed-width types instead of bare `int`.** CODESTYLE requires `S32`/`U32` in ported and engine
code. Thousands of hits across 1997 sources, so the rule only means anything applied to new
lines, and a diff-scoped check is a different and more fragile kind of tool.

**Line-count ratchets on the god files.** The roadmap's own growth table is the tempting
version: GAMEMENU.CPP is +1,771 lines since the import and PERSO.CPP +643. As a rule it taxes
every bug fix in those files to discourage new subsystems, which is not the behaviour the
roadmap asks for. The ownership ratchets in rules 4 and 5 catch the same drift at the place it
actually causes harm.

## Where the check runs

[format.yml](../../.github/workflows/format.yml) is the natural home. It already runs on every
push and pull request with no path filter, it is the existing whole-tree static gate for C and
C++, and it needs no counterpart in [docs-gate.yml](../../.github/workflows/docs-gate.yml).

[lint.yml](../../.github/workflows/lint.yml) is the wrong file despite covering the language
the checker would be written in. Its trigger is an allowlist of `**.sh`, `**.py` and
`.github/**`, chosen so that a C++ change spends no runner there. GitHub path filters are
per-workflow, not per-job, so adding `SOURCES/**` and `LIB386/**` would fire shellcheck,
actionlint and ruff on every engine commit to reach one job. That is the cost the comment at
the top of that file exists to avoid.

Two integration details follow from that. A workflow filtered by `paths` reports no check run
at all when it is skipped, which is the trap [docs-gate.yml](../../.github/workflows/docs-gate.yml)
exists to work around; an unfiltered workflow like format.yml sidesteps it. And the checker is
Python, so ruff already lints it under the existing lint job with no new configuration.

## Shape of the implementation

One script, `scripts/ci/check-arch.py`, with the rules as data at the top and no dependencies
beyond the standard library. Under 200 lines, well under a second over the tree, and runnable
by hand with the same command CI uses.

The two ratchet baselines live in that file as constants next to the rule they belong to,
rather than in a generated baseline file. An ownership PR then lowers a number in a diff a
reviewer reads, which is the same place the commit message already quotes it.

Every failure prints the file, the line, the rule and the sentence from CODESTYLE or AGENTS.md
that the rule comes from. A gate that only says "violation" sends people to read the checker,
and the checker is not the authority.

## Documentation the change owes

A rule that only exists in a checker is a trap: the contributor meets it as a build failure
with no prose behind it, and the prose it came from still reads as advice. So the enforcement
has to land next to the statement, in the same commit, in five places.

| Doc | Edit | Why it is obliged |
|---|---|---|
| [CODESTYLE.md](../../CODESTYLE.md) | A sentence at each rule the checker covers, naming it | The rule and its enforcement must not be discovered separately |
| [AGENTS.md](../../AGENTS.md) | A "When modifying X, do Y" row, and the check named in "Before considering done" beside `apply-format.sh` | Same table already routes every other invariant |
| [scripts/README.md](../../scripts/README.md) | A row in the `ci/` table | AGENTS.md: a script added under `scripts/` gets its row in the same commit |
| [docs/CI.md](../CI.md) | The `format.yml` row in the workflow map gains its second job | That table is the map of what CI runs |
| [Makefile](../../Makefile) | A target beside `format-check`, `docs-links`, `docs-symbols` | Every other CI check has one, and `make help` is where people look |

The CODESTYLE edit is the one with real content. Rules 2, 5 and 6 come from three separate
passages there, and each should say that the boundary is now checked and where. For rule 5 in
particular, "Where new code goes" currently explains why removing a header from DEFINES.H is
the expensive half of an ownership move; it should also say that the aggregation cannot grow
back, because that is the half a future contributor would otherwise undo by accident.

Two couplings to state rather than leave implicit.

**The rule 3 allowlist and the seam doc are the same claim in two places.** The per-module
label table in [ENGINE_GAME_SEAM.md](../ENGINE_GAME_SEAM.md) is what justifies each file
carrying a platform conditional. A file joining the allowlist should get its label there in the
same commit, or the allowlist slowly becomes a list of exceptions with no argument behind them.

**The plan's own status is an edit.** Its row in [docs/README.md](../README.md) moves from
Proposed to Implemented when the last phase lands, and the durable parts (the rule list, the
four rejected candidates and why) belong in CODESTYLE by then. A plan must never be the only
description of shipped behaviour.

### What is not owed

Recorded so the next reader does not add them out of diligence.

- **[TOOLING.md](../TOOLING.md) and `scripts/dev/check-tooling.sh`.** Those cover a new
  *external* tool. The checker is standard-library Python, which the repo already requires for
  `filter-format-files.py` and `check-docs-symbols.py`, so there is no row to add and nothing
  to probe for.
- **The pre-commit hook.** [scripts/git-hooks/pre-commit](../../scripts/git-hooks/pre-commit)
  deliberately checks *staged blobs*, file by file, so a partial stage still formats correctly.
  Rules 4 and 5 are whole-file counts and rule 6 is a property of a CMake target, so a per-file
  hook would either read the working tree, contradicting that design, or skip the ratchets in
  silence. The `make` target is the local path; CI is the gate.
- **[CONTRIBUTING.md](../../CONTRIBUTING.md).** Its Code style section covers running the
  formatter locally, and the arch check has no fixer to run. The CODESTYLE sentences plus the
  `make` target cover it.

### Verifying the doc edits

[docs-links.yml](../../.github/workflows/docs-links.yml) already polices both halves of this,
and both have a way of appearing to pass when they have not run.
`scripts/ci/check-docs-links.sh` feeds lychee from `git ls-files '*.md'`, so a new doc is
invisible until it is staged; the total-links count is the tell that it was read. And
`check-docs-symbols.py` verifies every `` `Foo()` in [FILE] `` claim against where the symbol
is actually defined, which is worth knowing before writing the CODESTYLE sentences, since they
will name entry points.

## Order of work

Three PRs, each self-contained.

1. **The script, the CI job, and the five clean rules**, plus every row in the table above.
   Nothing to fix, so the first PR is enforcement only and cannot be argued with on merit.
2. **The two console help strings**, then rule 7 joins the checker.
3. **`Console_SetUnknownCommandHandler` and the cheat registration**, then rule 6. Worth
   confirming the `console` library links standalone afterwards, which is the real prize: the
   linker enforcing the boundary makes the rule a backstop rather than the mechanism.
