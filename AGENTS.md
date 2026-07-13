# Agent guidance — LBA2 Classic Community

This document helps AI coding assistants (Cursor, Copilot, Claude, etc.) work effectively on this codebase. Read it before making changes or answering questions about the code.

## Principles

1. **Truth hierarchy and doc accuracy:** Code > project docs > external sources. Prefer `docs/GLOSSARY.md`, `docs/LIFECYCLES.md`, etc. over inference. When code and docs conflict, code wins — fix the docs in the same commit or PR (environment trumps willpower; maintainers may forget). Keep `docs/ASM_VALIDATION_PROGRESS.md` and `docs/ASM_TO_CPP_REFERENCE.md` current.

2. **Preserve the nature of the game:** Do not change gameplay, feel, or behavior by default. Fix bugs that alter behavior; do not "improve" behavior without explicit agreement.

3. **Optional features behind flags:** New behavior or features that change the experience must be opt-in (build options, config flags, runtime toggles). Default builds should match original behavior.

4. **Quality over speed:** Equivalence tests must pass. Do not relax tests to unblock. Prefer smaller, well-tested changes. Fix bugs before merging.

5. **Cross-platform priority:** Support Linux, macOS, and Windows (desktop) and Android (arm64-v8a / armeabi-v7a, mobile). Contributors and agents may be on any desktop host—do not assume Linux. Avoid platform-specific assumptions (paths, case sensitivity, toolchains). Prefer portable C/C++. No inline x86 in library code. The engine builds on SDL3, which supports [many more platforms](https://github.com/libsdl-org/SDL/blob/main/docs/README-platforms.md)—writing portable code today keeps that door open.

## Never

- Never relax equivalence tests or add approximate comparisons
- Never point a user-facing string at the repo. Help text, console command descriptions, log lines, on-screen messages and dialogs reach people who have only the binary: no `docs/*.md`, no source paths, no issue numbers. Point at `README.txt` (it ships) or, better, say the thing itself. `--help` in particular is read by players, not only by agents driving the harness.
- Never leak the session into a comment. Comments state the constraint or intent that outlives the merge, in the present tense, as if the code had always looked this way. No "used to", no "the old code assumed", no narrating the change you just made or the evidence that motivated it: that belongs in the commit message and the PR body, which is where a reader goes looking for it. Stripping the story must not strip the *why*, so keep the invariant, the hazard, or the non-obvious coupling the code can't show on its own. Bad: `/* a spelling that used to work would now be rejected */`. Good: `/* RES_DISCOVERY parses these with strcasecmp, so the validator must too, or it would reject a spelling that parser accepts. */`
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

## Golden rules

Operational complements to the principles above.

- **ASM is the source of truth.** All equivalence tests compare ASM vs C++ byte-for-byte. Use `ASSERT_ASM_CPP_EQ_INT` for scalars, `ASSERT_ASM_CPP_MEM_EQ` for buffers. See `docs/ASM_TEST_COVERAGE_AUDIT.md` for what "covered" means in practice (branches, side effects, edge inputs, ASM-invalid domains). This applies to ported engine code (LIB386, SOURCES/3DEXT). New SOURCES infrastructure with no ASM original (e.g. console, logging, control harness) is verified by host tests, not ASM equivalence.
- **Think like an archaeologist:** This codebase is a window into 1990s game dev at Adeline Software. Surface historical context when relevant. See `docs/FRENCH_COMMENTS.md` and `docs/ASCII_ART.md`.
- **Search before adding:** Check for existing implementations before adding new code; avoid duplication.
- **Pin fixes with a test or a repro hook.** When fixing a bug, first ask whether the affected logic is pure-data (parsing, serialization, math, projection, sort) — if yes, extracting it into a pure function and adding a host test is usually a small additional refactor and should be done as part of the fix. When automation is impractical (state, timing, UI), ship a manual repro hook instead (a console command, debug menu entry, or build flag). See [CONTRIBUTING.md "Doing good work here"](CONTRIBUTING.md#doing-good-work-here) for the rationale.

## Execution style (Karpathy-inspired)

Apply these behavior rules on every non-trivial task:

1. **Think before coding:** State assumptions explicitly. If intent is ambiguous, ask before implementing. Surface tradeoffs instead of silently picking one path.
2. **Simplicity first:** Prefer the minimum code and the minimum API needed to satisfy the request. Avoid speculative abstractions and "future-proofing" unless requested.
3. **Surgical changes:** Touch only files and lines needed for the requested outcome. Avoid drive-by refactors, formatting churn, or unrelated cleanup.
4. **Goal-driven execution:** Define concrete success criteria (preferably tests or reproducible checks), implement, then verify results before declaring done.

## Project overview

- **What:** Community fork of the LBA2 (Twinsen's Odyssey) engine for preservation and modernization.
- **Main work:** Porting x86 assembly to C++ while preserving exact behavior. ASM remains the source of truth.
- **Stack:** C++98 game logic, LIB386 engine libraries (C linkage), x86 ASM (UASM), SDL3, libsmacker.
- **Structure:** `SOURCES/` = game logic; `LIB386/` = engine (3D, audio, rendering); `docs/` = reference documentation.

## Build & test

- **Build:** `cmake -B build && cmake --build build`. Or use presets for your platform: `cmake --preset <preset> && cmake --build --preset <preset>` — `linux`, `macos_arm64`, `macos_x86_64`, `windows_ucrt64` (see `CMakePresets.json`).
- **Game data:** Retail files are not in the repo. See `docs/GAME_DATA.md`. Set `LBA2_GAME_DIR`, use `--game-dir`, or place data in `./data` / `../LBA2` relative to the repo. From anywhere in the clone: `./scripts/dev/build-and-run.sh` or `make run` (repo root).
- **Tests:** Run via Docker: `./run_tests_docker.sh`. Works on Linux, macOS (Docker + QEMU), and Windows (Docker Desktop). Tests require 32-bit x86 + UASM inside the container.
- **Host tests:** `make test` (or `ctest -L host_quick` after configure with `-DLBA2_BUILD_TESTS=ON -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF`). No Docker, no retail files. CI runs these on Linux, macOS, and Windows (see `.github/workflows/*.yml`).
- **Filter:** `./run_tests_docker.sh test_getang2d test_lirot3df`
- **Bisect:** `./run_tests_docker.sh --bisect` to find first divergent draw call
- **Before considering done:** Run `./run_tests_docker.sh` (or N/A if docs-only). If modifying formatted files: run `bash ./scripts/ci/apply-format.sh` (or enable the pre-commit hook — see "Code conventions"); CI also runs the format check.
- **When tests fail:** Do not relax tests. For ASM↔CPP: use `--bisect` to find first divergence, add debug traces to both ASM and CPP, extract failing inputs into a focused unit test, fix CPP to match ASM. For other bugs: read the relevant subsystem doc (AUDIO, MENU, DEBUG, etc.), map the code path, fix the bug, verify with build/tests.
- **Minimal build** (no audio/video): `-DSOUND_BACKEND=null -DMVIDEO_BACKEND=null` for quick iteration.
- **Switch render resolution at launch:** `--resolution WxH` (e.g. `--resolution 768x480`, `--resolution 1280x720`). Range `320x200` – `1920x1080`, width must be a multiple of 8. Default (no flag) is the compile-time `RESOLUTION_X/Y` (640x480). Useful for sweeping width-dependent bugs from one binary instead of rebuilding per width (see `docs/WIDESCREEN_HARDCODED_DIMS_AUDIT.md`). *Dev-mode for now*: the engine renders correctly, but the 2D UI is still anchored at 640x480 (Tier 2 work).
- **Drive it programmatically:** the CLI control harness boots, restores a save, runs console commands, advances N ticks, writes a JSON state snapshot / PNG, and exits, all in one invocation: `lba2cc --headless --no-autosave --load <save> --tick 30 --dump-state s.json --screenshot s.png --exit`. Use it to verify a runtime change without clicking through the game: compare `--dump-state` before/after, or diff a screenshot. Add `--fixed-dt 16` for byte-reproducible dumps so you can assert exact rather than eyeball. Needs retail data; `tests/automation/run.sh` runs the suite. Modal commands (`give <item>`, `playvideo`) hang a `--exit` run, see caveats. See [docs/CONTROL.md](docs/CONTROL.md).
- **Always pass `--headless` (and usually `--no-autosave`).** Without it the engine opens a window, and **the game pauses when that window loses focus**, so a run that loses focus gets a stopped clock and runs in parallel fight over it: six identical `--fixed-dt` runs windowed produced five different sim states, headless they were byte-identical. `--fixed-dt` only buys you determinism inside `--headless`. `--no-autosave` stops the run rewriting `autosave.lba` on your own install (explicit saves, including `savebug`, still work). Two more traps worth knowing: `--exec` fires on the first tick and *races* a `cube` change (which lands the next frame), so use `--exec-at <tick> "<cmds>"` when a command depends on the new scene; and the first frame after `--load` is a full redraw, which hides exactly the partial-frame rendering bugs (stale background, dirty-box) the harness is good at catching.
- **Drive game state, not just saves:** for state- or quest-gated interactions the harness can't reach by input (no movement/menu), use `teleport` / `teleport actor <n>` (position the hero), `varcube` / `vargame <n> [value]` (read or set the scene/game Life variables that gate quest progression), and `lifetrace <objN>` (log an NPC's Life script: comportement/track/zone + every `LF_` condition it evaluates). One `--exec` line can then reproduce an interaction (position + quest flags + script observability) instead of hunting for the exact playthrough save. See [docs/CONSOLE.md](docs/CONSOLE.md).

## When modifying X, do Y

| Modifying | Do | See |
|-----------|-----|-----|
| LIB386 C++ port | Check ASM_VALIDATION_PROGRESS Notes; add/update equivalence test | docs/ASM_VALIDATION_PROGRESS.md |
| SOURCES (game logic) | No ASM tests; use docs/GLOSSARY, LIFECYCLES for domain terms | docs/GLOSSARY.md, docs/LIFECYCLES.md |
| SOURCES/3DEXT/ | Same as LIB386; check ASM_TO_CPP_REFERENCE | docs/ASM_TO_CPP_REFERENCE.md |
| Adding ASM↔CPP test | Use `add_asm_cpp_test()`, include stress test, update `docs/ASM_VALIDATION_PROGRESS.md`; check `docs/ASM_TEST_COVERAGE_AUDIT.md` for the coverage rubric | docs/TESTING.md, docs/ASM_TEST_COVERAGE_AUDIT.md, .github/copilot-instructions.md |
| Audio/video | AIL in LIB386/AIL/; backends SDL, Miles, null | docs/AUDIO.md |
| Android-specific behaviour (JNI, TV detection, storage permission) | Put it behind `LIB386/SYSTEM/ANDROID.{CPP,H}` with a stub-on-desktop; keep callers `#ifdef`-free; `<jni.h>` in that TU only; LIB386 must not include `SOURCES/` | docs/ANDROID.md, docs/PLATFORM.md §8 |
| Debug tools | DEBUG_TOOLS (console is always available) | docs/DEBUG.md, docs/CONSOLE.md |
| Verifying a runtime change (scene, hero, inventory, render) | Drive the engine non-interactively and assert on dumped state / a screenshot instead of manual play | docs/CONTROL.md |
| Config / lba2.cfg | Keys, persistence, installer vs game, embedded default | docs/CONFIG.md, docs/GAME_DATA.md |
| Adding a log or diagnostic line | Prefer `Log_*` (`<SYSTEM/LOG.H>`) at the true severity for committed code; `Log_Debug` is off by default (all sinks), surfacing only at `--log-level`/`loglevel debug`. Throwaway `printf` is fine; don't commit it | "Logging" below, LIB386/H/SYSTEM/LOG.H |
| Code that reads retail HQR data or legacy save formats | Check the rule: never `sizeof(T)`-as-stride for fat structs; use a paired `T_DISK` or field-by-field serialization | docs/ABI.md |
| File with French comments or ASCII art | Preserve; add new comments alongside | docs/FRENCH_COMMENTS.md, docs/ASCII_ART.md |
| New subsystem or doc | Create docs/<name>.md; add to docs/README.md; update in same commit | docs/README.md |
| Any code that affects documented behavior | Update the doc in the same commit | Principle 1 |
| Fixing a bug | If the affected logic is pure-data, extract it to a pure function and add a host test. Otherwise add a manual repro hook (console command, debug flag) | CONTRIBUTING.md "Doing good work here" |
| Editing docs (any `.md`) | Sentence-case headings; bold for structure only (list-item / paragraph leads, table row labels), not mid-sentence emphasis; verify file/line refs; from `docs/`, link to source with `../SOURCES/...` / `../LIB386/...` | "Editing docs" below; CONTRIBUTING.md "Doing good work here" |

## Code conventions

Style rules and rationale (language dialect, indentation, types, C++98, preservation) live in [CODESTYLE.md](CODESTYLE.md) — read it before writing engine code. The agent-critical invariant: **ported original code stays C-style to mirror the ASM; new infrastructure with no ASM original may use conservative C++98 features.** Never modernize game code (see the Never list). When unsure, match the file you are editing.

Formatting mechanics (agent-operational):

- Formatting: clang-format with checked-in `.clang-format`; exclusions live in `.clang-format-ignore` (ASM, `LIB386/libsmacker/`, vendored `stb_*`, and a handful of legacy lookup-table files). When adding vendored third-party code or generated/hand-tuned lookup tables, add the path to `.clang-format-ignore` in the same commit — single source of truth for local scripts, CI, and the pre-commit hook.
- clang-format version: pinned once in `scripts/ci/clang-format-select.sh` (`CLANG_FORMAT_MAJOR`, currently 18). Install that exact version (`clang-format-18`) so local formatting matches CI; the scripts refuse to run a different major rather than silently disagreeing with CI.
- Pre-commit hook (optional, recommended): `git config core.hooksPath scripts/git-hooks` once per clone. It runs clang-format `--dry-run` on staged C/C++ files (respecting `.clang-format-ignore`) and blocks commits with violations. Fix with `bash ./scripts/ci/apply-format.sh && git add -u`. Bypass with `git commit --no-verify` or `SKIP_FORMAT=1` when warranted.

## Logging

One pipe: every log call fans out to `adeline.log` (always), stderr, and the in-engine console. API in [LIB386/H/SYSTEM/LOG.H](LIB386/H/SYSTEM/LOG.H); design and history in [docs/LOGGING_UNIFICATION.md](docs/LOGGING_UNIFICATION.md). This is about ergonomics, not ceremony; it isn't meant to police throwaway debugging.

- **Pick the true severity.** `Log_Info` / `Log_Warn` / `Log_Error`, plus `Log_Raw` for verbatim lines (no severity tag).
- **One master level gates every sink.** A single global level (default `INFO`) is checked before any sink, so it controls `adeline.log`, the terminal, and the console together. Set it with `--log-level <debug|info|warn|error>`, the `LBA2_LOG_LEVEL` env var, or the `loglevel` console command (`Log_SetLevel` in code). Structural framing (the identity banner and section markers, `Log_Banner` / `Log_BeginSection`) bypasses the level so a pasted log stays self-describing even when raised; `Log_Raw` verbatim lines obey the level like normal records.
- **`Log_Debug` is for low-volume, meaningful detail** (a save written, a scene/cube change, an init result), not per-frame or per-object output. Off by default; it surfaces everywhere at once (`adeline.log`, terminal, console) when the level is lowered to `debug` (`--log-level debug` / `loglevel debug`). The test: someone who turns debug on should get signal, not a wall of text. If a line can fire many times a second it does not belong here (see the trace bullet). Varargs like `printf`, so there's no friction reason to prefer `printf` in committed code.
- **Throwaway debugging is your business** — a quick `printf` while iterating is fine; just don't commit it.
- **Spammy per-frame / per-subsystem tracing is a runtime toggle, not a severity.** This is the "trace" tier: high-volume diagnostics you switch on from the console only while chasing a specific problem (the `audio` and `video log` switches are the pattern; `sfxLogEnabled` / `musicLogEnabled` guard the call sites, silent by default). There is deliberately no `TRACE` log level: such traces are usually temporary and pulled once the bug is found, so a persistent severity would be YAGNI. Don't route this kind of chatter through `Log_Debug`.
- **No log categories/channels** (deliberately — YAGNI). To filter by area, prefix the message text (`[SFX]`, `[CD]`) and grep the file.
- `LogPrintf` / `LogPuts` are legacy shims onto the same fan-out — fine where they are; no need to add new calls.
- **The stderr sink always emits, so headless runs are visible.** On a real TTY it's ANSI-coloured; when redirected (harness, CI, `2>file`) it falls back to plain, severity-tagged lines with the same info and no escape codes. So a piped `--load ... --tick ... --exit` run shows the boot log and any `Log_*` output inline on stderr; you don't have to open `adeline.log`. For a debug trace in the harness, add `--log-level debug`. `stdout` stays a pure data channel (`--dump-state`, `--exec` mirror), never logs.
- **Before the sinks exist** (early boot), `Log_*` falls back to stderr so errors still surface.

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

## Further reading

- [LICENSE](LICENSE) — GPL v2; project license
- [CODESTYLE.md](CODESTYLE.md) — Canonical code-style rules: language dialect by zone, indentation, types, C++98, preservation
- [CONTRIBUTING.md](CONTRIBUTING.md) — Contribution workflow, formatter mechanics, PR workflow
- [.github/copilot-instructions.md](.github/copilot-instructions.md) — Detailed ASM↔CPP workflow, polyrec debugging, test patterns, common pitfalls
- [docs/FEATURE_WORKFLOW.md](docs/FEATURE_WORKFLOW.md) — Reasoning and docs for big features (console, headless, menu, camera)
- [docs/README.md](docs/README.md) — Full documentation index
