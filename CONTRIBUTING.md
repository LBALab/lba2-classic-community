# How to Contribute

This is a community project. Maintainers and contributors come and go as life
allows — that's fine and expected. The goal is a sustainable, friendly
place to work on an old game we love. Pick something that interests you,
move at your own pace, and ship it when it's ready.

A few notes that keep it sustainable:

- **Keep PRs focused.** One topic per PR. Sweeping rewrites or
  whole-subsystem changes (e.g. "port everything to Rust", reorganizing a
  whole subdirectory) deserve a heads-up on
  [Discord](https://discord.gg/jsTPWYXHsh) or an issue *before* you start
  — reviewer time is the scarce resource, not code.
- **AI assistants are welcome** (Cursor, Copilot, Claude, etc.). The bar
  is the same as for any contributor: the code has to be correct, the
  scope has to match what the PR title claims, and ASM equivalence still
  has to hold. Point your assistant at [AGENTS.md](AGENTS.md) for project
  context, principles, and the "never" list.
- **Ask early when in doubt.** A short question on
  [Discord](https://discord.gg/jsTPWYXHsh) beats a week of work in the
  wrong direction.

## Doing good work here

A few principles. The *why* matters more than the rule — once you
understand the why, you can apply judgment in cases the rule doesn't
cover. None of this is unique to this project; it's how good engineering
works generally. It's written down because this is a community project
where reviewer time is scarce, and these habits are what keep quality
high *without* anyone having to police it.

- **Own what you ship.** If you used an AI assistant, you are still the
  author. Read the diff yourself before opening the PR; you should be
  able to explain every line and defend every decision under review.
  *Why:* outsourcing typing is fine — outsourcing *understanding* is how
  subtle bugs land in old code. Your name on the PR is a commitment.

- **Actually test your change.** Build it. Run the game (or the relevant
  test). Walk through the path your change touches. Then try one path it
  doesn't touch, in case you broke something next door. *Why:* "the
  automated tests pass" is necessary, not sufficient. This codebase is
  25 years old and full of failure modes nothing has ever automated. You
  are the last pair of eyes before review.

- **Write tests when you reasonably can.** Especially for new logic,
  regressions you just fixed, and anything in `LIB386/` where ASM
  equivalence is the bar. *Why:* a test you write today catches the bug
  your future self introduces in six months, on a platform you don't
  own, for a contributor you've never met. That's a great trade.

- **CI is on your side.** When it fails, read the log and fix the cause.
  Don't disable a check, skip a hook (`--no-verify`), or work around the
  failure to "unblock" yourself. *Why:* CI catches what you forgot to
  check on your machine — that's its whole job, and it's free. Fighting
  it just defers the same problem to whoever merges after you, often on
  a platform you didn't test.

- **Explain *why*, not *what*.** In PR descriptions, commit messages,
  and code comments alike. *Why:* the diff already shows what changed.
  What it can't show is why this approach over the obvious alternative,
  what constraint you hit, or what you considered and rejected. That's
  the part future contributors (and your future self) will need.

- **Smaller is better.** The smallest change that solves the problem is
  easier to review, easier to revert if it turns out to be wrong, and
  easier to come back to after a break. *Why:* every line is a
  maintenance cost. Resist "while I'm here" cleanups; open a separate PR
  instead.

- **Ship a test, or ship a repro hook.** Bugs in pure-data code paths
  (parsing, serialization, math, projection, sort) are almost always
  cheap to test once you extract the affected logic into a pure function
  — that small refactor is usually worth doing as part of the fix, with
  a host test pinning it. See `tests/3D/test_sintab.cpp` and the
  savegame corpus harness as patterns. When automation genuinely isn't
  realistic (state machines, input timing, UI flow), at least ship a
  **manual repro hook** alongside the fix: a console command, a debug
  menu entry, or a build flag that re-triggers the affected path. PR
  #66's `credits [0|1]` console command is the canonical example.
  *Why:* future you (or someone else) will debug this same area again.
  A test pins it forever; a repro hook saves an hour of figuring out
  how to re-trigger the bug. Both beat nothing.

## Reporting Crashes and Bugs

If you observe a bug you can report it [here](https://github.com/LBALab/lba2-classic-community/issues/new).

## Proposing Features

If you have an idea, you can [create a feature request](https://github.com/LBALab/lba2-classic-community/issues/new) to suggest a new feature.

## Contributing Code

1. Fork the repository
2. Create a feature branch from `main`
3. Make your changes
4. Run `./run_tests_docker.sh` before submitting (or N/A if docs-only). CI will catch failures, but local run saves round-trips. PRs also run **host** discovery tests (`test_res_discovery`) on **Linux, macOS, and Windows** plus **format**; full ASM equivalence is the Docker workflow on Linux.
5. Submit a pull request

PRs and pushes run build checks on **Linux**, **Windows** (MSYS2 UCRT64), and **macOS** (arm64); see `.github/workflows/`. Equivalence tests still run in Docker on Linux only (`test.yml`).

### Setting Up a Development Environment

See the [README](README.md) for prerequisites and build instructions. In short:

```bash
cmake -B build
cmake --build build
```

The default build includes SDL3-based audio and Smacker FMV playback. Override with `-DSOUND_BACKEND=null -DMVIDEO_BACKEND=null` for a minimal build; see the build options table in the README.

To **run** the game you need retail data (not in the repo); see [docs/GAME_DATA.md](docs/GAME_DATA.md).

### Code Style

The original codebase uses tabs for indentation. There is an ongoing effort to migrate to 4 spaces -- new contributions should use 4 spaces for indentation.

The codebase mixes C and C++ (C++98) along with x86 assembly (UASM). Please keep changes consistent with the style of the file you are modifying.

For C and C++ files, the repository now uses `clang-format` with a checked-in style file. In VS Code, workspace settings enable format-on-save for C and C++ when the recommended `ms-vscode.cpptools` extension is installed.

For local formatting work, use the repo scripts:

```bash
bash ./scripts/ci/check-format.sh
bash ./scripts/ci/apply-format.sh
```

`check-format.sh` verifies the tracked C/C++ files that are enforced in CI. `apply-format.sh` applies that same formatter policy to tracked clean files in your worktree.

If you use VS Code, install the workspace recommendations from `.vscode/extensions.json`. The current recommended set is `ms-vscode.cpptools`, `ms-vscode.cmake-tools`, and `EditorConfig.EditorConfig`.

`EditorConfig.EditorConfig` is recommended because this repository also has files that are not auto-formatted by `clang-format`. It makes VS Code honor the checked-in `.editorconfig` for baseline indentation rules, which keeps C/C++ files on 4 spaces and preserves tab-based indentation in ASM and related files.

ASM files are intentionally excluded from automatic formatting, and `LIB386/libsmacker/` is kept out of formatting because it is third-party code.

A small set of preservation-sensitive or macro-heavy files is also excluded from automatic formatting where `clang-format` does not produce stable results yet. Keep those files manual until they are split up or annotated for safer tooling.

The enforced exclusion list lives in `.clang-format-ignore`, and the CI/local scripts under `scripts/ci/` read that file directly.

If you are doing the one-time whitespace migration or reviewing it locally, keep it as a dedicated formatting-only commit and then configure git blame to ignore that commit:

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

After the bootstrap formatting commit exists, add its SHA to `.git-blame-ignore-revs`. Do not mix semantic changes into that commit.

### Preservation of Original Code

This project values preservation. When modifying original source files:

- Do not remove original French comments -- they are part of the codebase's history
- Do not remove or alter ASCII art banners in source files
- If you need to add clarifying comments, add them alongside the originals rather than replacing them
- When adding or changing documentation about history or culture, attribute to the **original codebase (lba2-classic)** and note when content was preserved during an ASM→C++ port in this fork

## Contributing to the Documentation

You can also help build the official documentation here: https://github.com/LBALab/lba-classic-doc
