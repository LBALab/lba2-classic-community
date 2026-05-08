# Agent Guidance — LBA2 Classic Community

This document helps AI coding assistants (Cursor, Copilot, Claude, etc.) work effectively on this codebase. Read it before making changes or answering questions about the code.

## Principles

1. **Truth hierarchy:** Code > project docs > external sources. Prefer `docs/GLOSSARY.md`, `docs/LIFECYCLES.md`, etc. over inference. When code and docs conflict, code wins—then update the docs.

2. **Document accuracy:** Docs drift from code over time. When you change behavior, update the relevant docs in the same commit or PR—environment trumps willpower; maintainers may forget. Keep `docs/ASM_VALIDATION_PROGRESS.md` and `docs/ASM_TO_CPP_REFERENCE.md` current. If you notice code and docs conflict, fix the docs (code wins).

3. **Preserve the nature of the game:** Do not change gameplay, feel, or behavior by default. Fix bugs that alter behavior; do not "improve" behavior without explicit agreement.

4. **Optional features behind flags:** New behavior or features that change the experience must be opt-in (build options, config flags, runtime toggles). Default builds should match original behavior.

5. **Quality over speed:** Equivalence tests must pass. Do not relax tests to unblock. Prefer smaller, well-tested changes. Fix bugs before merging.

6. **Cross-platform priority:** Support Linux, macOS, and Windows. Contributors and agents may be on any of these—do not assume Linux. Avoid platform-specific assumptions (paths, case sensitivity, toolchains). Prefer portable C/C++. No inline x86 in library code. The engine builds on SDL3, which supports [many more platforms](https://github.com/libsdl-org/SDL/blob/main/docs/README-platforms.md)—writing portable code today keeps that door open.

## Never

- Never relax equivalence tests or add approximate comparisons
- Never remove French comments or ASCII art
- Never add `__asm__` / inline x86 in LIB386 C++ files
- Never change gameplay, timing, or feel without explicit approval
- Never assume platform; never add unapproved dependencies
- Never "modernize" C++98 to C++11+ in game code (tests may use newer)
- Never add game assets (art, audio, models, textures) to the repo—they are not redistributable
- Never hardcode API keys, credentials, or secrets
- Never add GPL-incompatible code or dependencies; respect the project license (GPL v2). When copying code from elsewhere, ensure compatibility and proper attribution.
- Never run clang-format on ASM, `LIB386/libsmacker/`, or other excluded files
- Do only what was asked; avoid scope creep (refactoring, "improving" unrelated code)
- When the user's intent is ambiguous, ask before proceeding—do not guess.

## Golden Rules

- **ASM is the source of truth.** All equivalence tests compare ASM vs C++ byte-for-byte. Use `ASSERT_ASM_CPP_EQ_INT` for scalars, `ASSERT_ASM_CPP_MEM_EQ` for buffers.
- **Preservation:** French comments, ASCII art, attribution—these are historical artifacts. Add clarifying comments alongside originals; do not replace them.
- **Documentation first:** Use `docs/GLOSSARY.md` for domain terms, `docs/LIFECYCLES.md` for lifecycles, `docs/ASM_TO_CPP_REFERENCE.md` for port status, `docs/ASM_VALIDATION_PROGRESS.md` for test coverage, and `docs/ASM_TEST_COVERAGE_AUDIT.md` for what "covered" means in practice (branches, side effects, edge inputs, ASM-invalid domains).
- **Think like an archaeologist:** This codebase is a window into 1990s game dev at Adeline Software. Surface historical context when relevant. See `docs/FRENCH_COMMENTS.md` and `docs/ASCII_ART.md`.
- **Verify before claiming:** Run build or tests to confirm changes work; do not claim something works without verification.
- **Search before adding:** Check for existing implementations before adding new code; avoid duplication.
- **Say when unsure:** If uncertain whether a change is correct, say so. Prefer "I'm not certain" over confident but wrong.
- **Pin fixes with a test or a repro hook.** When fixing a bug, first ask whether the affected logic is pure-data (parsing, serialization, math, projection, sort) — if yes, extracting it into a pure function and adding a host test is usually a small additional refactor and should be done as part of the fix. When automation is impractical (state, timing, UI), ship a manual repro hook instead (a console command, debug menu entry, or build flag). See [CONTRIBUTING.md "Doing good work here"](CONTRIBUTING.md#doing-good-work-here) for the rationale.

## Execution Style (Karpathy-Inspired)

Apply these behavior rules on every non-trivial task:

1. **Think before coding:** State assumptions explicitly. If intent is ambiguous, ask before implementing. Surface tradeoffs instead of silently picking one path.
2. **Simplicity first:** Prefer the minimum code and the minimum API needed to satisfy the request. Avoid speculative abstractions and "future-proofing" unless requested.
3. **Surgical changes:** Touch only files and lines needed for the requested outcome. Avoid drive-by refactors, formatting churn, or unrelated cleanup.
4. **Goal-driven execution:** Define concrete success criteria (preferably tests or reproducible checks), implement, then verify results before declaring done.

## Project Overview

- **What:** Community fork of the LBA2 (Twinsen's Odyssey) engine for preservation and modernization.
- **Main work:** Porting x86 assembly to C++ while preserving exact behavior. ASM remains the source of truth.
- **Stack:** C++98 game logic, LIB386 engine libraries (C linkage), x86 ASM (UASM), SDL3, libsmacker.
- **Structure:** `SOURCES/` = game logic; `LIB386/` = engine (3D, audio, rendering); `docs/` = reference documentation.

## Build & Test

- **Build:** `cmake -B build && cmake --build build`. Or use presets for your platform: `cmake --preset <preset> && cmake --build --preset <preset>` — `linux`, `macos_arm64`, `macos_x86_64`, `windows_ucrt64` (see `CMakePresets.json`).
- **Game data:** Retail files are not in the repo. See `docs/GAME_DATA.md`. Set `LBA2_GAME_DIR`, use `--game-dir`, or place data in `./data` / `../LBA2` relative to the repo. From anywhere in the clone: `./scripts/dev/build-and-run.sh` or `make run` (repo root).
- **Tests:** Run via Docker: `./run_tests_docker.sh`. Works on Linux, macOS (Docker + QEMU), and Windows (Docker Desktop). Tests require 32-bit x86 + UASM inside the container.
- **Host tests:** `make test` (or `ctest -L host_quick` after configure with `-DLBA2_BUILD_TESTS=ON -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF`). No Docker, no retail files. CI runs these on Linux, macOS, and Windows (see `.github/workflows/*.yml`).
- **Filter:** `./run_tests_docker.sh test_getang2d test_lirot3df`
- **Bisect:** `./run_tests_docker.sh --bisect` to find first divergent draw call
- **Before considering done:** Run `./run_tests_docker.sh` (or N/A if docs-only). If modifying formatted files: `clang-format -i` on staged C/C++ files (works on all platforms), or CI runs format check.
- **When tests fail:** Do not relax tests. For ASM↔CPP: use `--bisect` to find first divergence, add debug traces to both ASM and CPP, extract failing inputs into a focused unit test, fix CPP to match ASM. For other bugs: read the relevant subsystem doc (AUDIO, MENU, DEBUG, etc.), map the code path, fix the bug, verify with build/tests.
- **Minimal build** (no audio/video): `-DSOUND_BACKEND=null -DMVIDEO_BACKEND=null` for quick iteration.

## When Modifying X, Do Y

| Modifying | Do | See |
|-----------|-----|-----|
| LIB386 C++ port | Check ASM_VALIDATION_PROGRESS Notes; add/update equivalence test | docs/ASM_VALIDATION_PROGRESS.md |
| SOURCES (game logic) | No ASM tests; use docs/GLOSSARY, LIFECYCLES for domain terms | docs/GLOSSARY.md, docs/LIFECYCLES.md |
| SOURCES/3DEXT/ | Same as LIB386; check ASM_TO_CPP_REFERENCE | docs/ASM_TO_CPP_REFERENCE.md |
| Adding ASM↔CPP test | Use `add_asm_cpp_test()`, include stress test, update `docs/ASM_VALIDATION_PROGRESS.md`; check `docs/ASM_TEST_COVERAGE_AUDIT.md` for the coverage rubric | docs/TESTING.md, docs/ASM_TEST_COVERAGE_AUDIT.md, .github/copilot-instructions.md |
| Audio/video | AIL in LIB386/AIL/; backends SDL, Miles, null | docs/AUDIO.md |
| Debug tools | DEBUG_TOOLS (console is always available) | docs/DEBUG.md, docs/CONSOLE.md |
| Config / lba2.cfg | Keys, persistence, installer vs game, embedded default | docs/CONFIG.md, docs/GAME_DATA.md |
| Code that reads retail HQR data or legacy save formats | Check the rule: never `sizeof(T)`-as-stride for fat structs; use a paired `T_DISK` or field-by-field serialization | docs/ABI.md |
| File with French comments or ASCII art | Preserve; add new comments alongside | docs/FRENCH_COMMENTS.md, docs/ASCII_ART.md |
| New subsystem or doc | Create docs/<name>.md; add to docs/README.md; update in same commit | docs/README.md |
| Any code that affects documented behavior | Update the doc in the same commit | Principle 2 |
| Fixing a bug | If the affected logic is pure-data, extract it to a pure function and add a host test. Otherwise add a manual repro hook (console command, debug flag) | CONTRIBUTING.md "Doing good work here" |
| Editing docs (any `.md`) | Sentence-case headings; bold for structure only (list-item / paragraph leads, table row labels), not mid-sentence emphasis; verify file/line refs; from `docs/`, link to source with `../SOURCES/...` / `../LIB386/...` | "Editing docs" below; CONTRIBUTING.md "Doing good work here" |

## Code Conventions

- Indentation: 4 spaces (C/C++); tabs preserved in ASM
- Formatting: clang-format with checked-in `.clang-format`; ASM and `LIB386/libsmacker/` excluded
- Types: S32, U32 from `LIB386/H/SYSTEM/ADELINE_TYPES.H`
- C++98 for game code; tests may use C11/C++11

## Editing docs

For tone and engineering principles, see [CONTRIBUTING.md "Doing good work here"](CONTRIBUTING.md#doing-good-work-here). The rules below are the doc-formatting standard on top of that.

**User-facing vs agent-facing.** README, CONTRIBUTING, CHANGELOG, and most of `docs/` are user-facing — tone matters and the rules below apply. AGENTS.md and `.github/copilot-instructions.md` are agent-facing — accuracy and structure for agents to parse matter most; tone and bold density matter less.

**Formatting:**

- Headings: sentence case. Proper nouns, place names, and acronyms keep their capitals (e.g. "MSYS2", "ASM", "Citadel Island", "GitHub release").
- Bold for structure, not emphasis. Keep list-item leads (`- **Foo:** description`), paragraph leads (`**Note:**`, `**Why:**`), and table row labels. Strip mid-sentence emphasis on inline code, product names, and short noun phrases.
- Italics for semantic markers (`*why*`, `*what*`) are fine. Don't italicise for decoration.
- Leave smart quotes (`"…"`) and `--` (double-hyphen) dashes alone — don't normalise.
- Reflow paragraph hard-wraps for consistency: prefer one paragraph per line and one item per list-row. If a doc mixes wrapped and unwrapped paragraphs, reflow.

**Accuracy (always pair with formatting):**

- Verify file paths, line numbers, function names, and command flags against the working tree before believing them.
- From `docs/`, links to `SOURCES/` or `LIB386/` need the `../` prefix (`[…](../SOURCES/X.CPP)`). Common breakage spot.
- Cross-check claims about CMake options, presets, and CI workflows against `CMakeLists.txt`, `CMakePresets.json`, and `.github/workflows/`.

**Special cases:**

- Verbatim attributed external content (e.g. [docs/COMPILER_NOTES.md](docs/COMPILER_NOTES.md)): leave entirely as-is.
- Curated content (`docs/FRENCH_COMMENTS.md`, `docs/ASCII_ART.md`, `docs/ASM_VALIDATION_PROGRESS.md`): light touch only — H1 + the project's own framing prose.

## Commit & PR conventions

The **PR title** drives the changelog. `git-cliff` parses it from the
commit log on `main` regardless of which merge mode you pick (merge,
squash, or rebase), so the title has to follow
[Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<optional scope>): <summary>
```

Allowed types:

| Type       | Use for                                                     |
|------------|-------------------------------------------------------------|
| `feat`     | New feature visible to players or contributors              |
| `fix`      | Bug fix                                                     |
| `port`     | ASM → C++ port work (fork-specific, distinct from refactor) |
| `perf`     | Performance improvement, no behavior change                 |
| `refactor` | Code restructuring, no behavior change                      |
| `docs`     | Documentation only                                          |
| `test`     | Adding or tightening tests                                  |
| `build`    | Build system, CMake, presets                                |
| `ci`       | CI workflow plumbing (matrix, runners, labels). For user-facing CI work like a release pipeline that ships binaries, prefer `feat:` so it lands in **Added** where end-users look. |
| `chore`    | Housekeeping — **skipped from the public changelog**        |

Scope is optional but useful (`fix(credits): ...`, `port(SORT): ...`).

A lightweight CI check (`.github/workflows/pr-title.yml`) verifies the PR
title format. **Individual commits inside the PR can be free-form** — only
the PR title is enforced. Contributors do not need to edit `CHANGELOG.md`;
it is regenerated at release time. Use `chore:` for any PR you want kept
out of the public changelog (formatting, tooling, internal cleanups).
See [docs/RELEASING.md](docs/RELEASING.md).

**Do not include `(#issue_num)` in the PR title.** When a PR is
squash-merged, GitHub appends `(#PR_num)` to the resulting commit subject
— if the title already contained `(#issue_num)`, the commit ends up with
both: e.g. `fix(credits): ... (#65) (#66)`. Reference issues in the PR
*body* instead, using GitHub's standard keywords (`Closes #65`,
`Fixes #65`, `Refs #65`).

## Further Reading

- [LICENSE](LICENSE) — GPL v2; project license
- [CONTRIBUTING.md](CONTRIBUTING.md) — Code style, preservation, PR workflow
- [.github/copilot-instructions.md](.github/copilot-instructions.md) — Detailed ASM↔CPP workflow, polyrec debugging, test patterns, common pitfalls
- [docs/FEATURE_WORKFLOW.md](docs/FEATURE_WORKFLOW.md) — Reasoning and docs for big features (console, headless, menu, camera)
- [docs/README.md](docs/README.md) — Full documentation index
