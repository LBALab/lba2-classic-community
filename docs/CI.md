# Continuous integration

GitHub Actions workflows live under `.github/workflows/`. They fall into
two tiers:

- **Validation** — runs on every push and pull request. Builds the engine
  on all three host platforms, runs the host test suite, the Docker ASM
  equivalence suite, the formatter, and the PR-title linter.
- **Release** — runs on `v*` tags and on pushes to `main`. Builds and
  packages the distributable artifacts. Documented in
  [RELEASING.md](RELEASING.md); this doc only summarizes how it connects
  to the validation tier.

This doc covers the validation tier and the cross-cutting machinery that
ties both tiers together — path filtering, the docs-only gate, and the
`main` branch protection that makes the gate necessary.

## Workflow map

| Workflow | Tier | Trigger | Job(s) | What it does |
|---|---|---|---|---|
| `linux.yml` | Validation | push, PR, dispatch | `build`, `build-clang`, `build-demo`, `sanitize`, `warnings` | Configure (`linux` preset), build `lba2cc` + `host_tests`, run `ctest -L host_quick`; the same through clang and for the demo SKU; the host tests once more under Address + UndefinedBehavior sanitizers; and a `-Wall -Wextra` build compared against the warning baseline |
| `macos.yml` | Validation | push, PR, dispatch | `build` | Same, on `macos-latest` (`macos_arm64` preset) |
| `windows.yml` | Validation | push, PR, dispatch | `build` | Same, on Windows MSYS2 UCRT64 (`windows_ucrt64` preset) |
| `test.yml` | Validation | push, PR, dispatch | `test` | Docker: `./run_tests_docker.sh` — full ASM↔C++ equivalence suite (Linux only, slow) |
| `format.yml` | Validation | push, PR, dispatch | `check-format`, `check-arch` | `scripts/ci/check-format.sh` (clang-format), and `scripts/ci/check-arch.py` for the architecture boundaries CODESTYLE.md and AGENTS.md state. Deliberately unfiltered: see the header comment there for why the architecture check is not in `lint.yml` |
| `lint.yml` | Validation | push, PR (shell/Python/workflow paths), dispatch | `shellcheck`, `actionlint`, `ruff` | Lints the non-C++ surface. Settings live in `.shellcheckrc` and `ruff.toml`; tool versions are pinned in the workflow |
| `pr-title.yml` | Validation | PR | `lint` | Conventional-commit lint on the PR title |
| `docs-gate.yml` | Validation | push, PR | `build`, `test` | No-op stand-ins for the required `build`/`test` checks on docs-only changes — see [below](#docs-only-gate) |
| `docs-links.yml` | Validation | push, PR, weekly | `links`, `external` | `scripts/ci/check-docs-links.sh` and `scripts/ci/check-docs-symbols.py`. Not path-filtered: a doc reference breaks from either side, since renaming a doc breaks source comments. `external` is weekly-only; see [below](#documentation-links) |
| `release-*.yml` | Release | `v*` tags, dispatch | `build` → `release` | Per-platform tag releases |
| `reusable-build-*.yml` | Release | `workflow_call` | `build` | Shared build+package steps, called by the release workflows |
| `release-latest.yml` | Release | push to `main` | build legs → `release` | Rolling `latest` pre-release |

Host build jobs (`linux`/`macos`/`windows`) need neither retail game
files nor Docker. The Docker job (`test.yml`) builds a 32-bit UASM image
and replays polyrec captures; it does not run the host discovery tests.

`linux.yml`'s `sanitize` job re-runs the host tests against the
`linux_sanitize` preset, which builds under AddressSanitizer and
UndefinedBehaviorSanitizer together. The suite was clean under both when
the job landed, so a red result means a change introduced a heap overflow,
a use-after-free, a leak, or undefined arithmetic. It builds `host_tests`
only: the `build` job already covers the game target, and no CI job has the
retail data needed to run it.

Two flags in that preset are load-bearing and worth not "tidying away".

`-fno-sanitize-recover=undefined` turns a UBSan finding into an abort.
UBSan's default is to print a diagnostic and continue, which exits zero:
the run stays green and the finding scrolls past in the log. Without this
flag the job is a decoration rather than a gate. If you want to *explore*
rather than gate, rebuild without it so a run reports every site instead of
stopping at the first.

`-fno-sanitize=alignment` switches off the misaligned-access check. That
one is not noise-suppression, it is a deferred decision: a headless
playthrough under UBSan reports 326 distinct misaligned load/store/member
sites, and every one of them is the same 1997 pattern of reading a packed
file or bytecode buffer through a cast pointer. `LbaReadWord` and friends in
`SAVEGAME.CPP`, `GET_S16` in `DISKFUNC.CPP` and `FICHE.CPP`, and structs
overlaid directly onto scene data in `OBJECT.CPP`. Fixing them is a real
piece of work with no observed x86 symptom, so the check stays off until
someone takes the class on deliberately; leaving it on would mean the gate
could never be green. Everything else UBSan checks is on, and the
playthrough found zero of it.

## The warning baseline

`-Wall -Wextra` is not enabled in any normal build. Turning it on reports 222
diagnostics, and a contributor who has just introduced the 223rd will never
find it in that list; a wall of pre-existing noise trains people to stop
reading warnings at all.

So `linux.yml`'s `warnings` job builds with them on and compares the result
against [`scripts/ci/warnings-baseline.txt`](../scripts/ci/warnings-baseline.txt).
The count can fall. It cannot rise. Clearing entries out of that file is the
unit of cleanup work: small, reviewable, and visibly monotonic.

```bash
scripts/ci/warning-baseline.sh            # compare (what CI runs)
scripts/ci/warning-baseline.sh --update   # regenerate after fixing warnings
```

Three decisions in that script are worth knowing before changing it.

**The baseline is keyed on (file, flag), never on line numbers.** A line-keyed
baseline is invalidated by any edit above a warning, so it would need
regenerating constantly, and a file people regenerate without reading is not a
gate. A per-file count only moves when the number of warnings in that file
actually moves.

**Counting is over unique `file:line:column:flag`, not over raw output lines.**
A warning in a header is re-reported by every translation unit that includes it,
so the raw count changes when an unrelated file adds an include.

**Three checks are suppressed**, with the count each contributed when the
baseline was introduced: `unknown-pragmas` (138, Watcom `#pragma aux` register
conventions, dead text to gcc and clang), `unused-parameter` (72, a faithful
port keeps a signature even where this build ignores an argument), and
`missing-field-initializers` (10, deliberate partial aggregate initialisation).
Everything else `-Wall -Wextra` reports is gated.

Warning sets move between compiler releases, so the baseline records the gcc it
was generated with and the script says so when the current compiler differs.
A version mismatch shows up as a wall of `NEW` lines that look like a
regression and are not one.

## Triggers: push and pull request

The four build/test workflows use `on: push` **with no branch filter**
plus `on: pull_request` and `workflow_dispatch`. Two consequences worth
knowing:

- **Every branch builds on push.** Pushing any branch — not just `main`
  or a PR branch — fires the validation workflows. A PR branch therefore
  gets two runs per push (one `push`, one `pull_request`); checks are
  keyed by commit SHA so the PR shows one consolidated set.
- **Merging `main` into a feature branch re-runs full CI.** A `push`
  event evaluates its path filter against *all files changed by the
  commits in the push*. When you merge `main` into a branch, the push
  carries every commit `main` advanced by, so the path filter sees source
  changes and the build runs — even if the PR's own diff is docs-only.
  **Rebase instead of merge** to keep a docs-only branch docs-only; the
  rebased push then carries only your own commits.

`format.yml` has no path filter at all — it is a ~30 s check and is
cheap enough to run on everything, including docs PRs.

## Path filtering

The build/test workflows skip changes that cannot affect build or test
output. `linux.yml`, `macos.yml`, and `windows.yml` share one
`paths-ignore` set; `test.yml` extends it.

The shared set (defined once per file via a within-file YAML anchor,
`&doc-paths` / `*doc-paths`, applied under both `push:` and
`pull_request:`):

```yaml
- '**.md'
- 'docs/**'
- 'LICENSE'
- '.gitignore'
- '.github/ISSUE_TEMPLATE/**'
- '.vscode/**'
- '.editorconfig'
- '.git-blame-ignore-revs'
- 'cliff.toml'
```

`test.yml` ignores a wider set on top of this — release/packaging files,
sibling CI workflows, `Makefile`, `scripts/dev/**`, `SOURCES/WIN/**` —
because the Docker ASM suite is the heaviest leg and none of those enter
its build. Its header comment lists exactly what is and is not safe to
add; read that before extending it.

The same shared set is also mirrored in the `push: branches: [main]`
trigger of `release-latest.yml`, so a docs-only commit to `main` does not
rebuild the rolling pre-release.

`paths-ignore` is evaluated differently per event: for `push` it is the
files changed by the push's commits; for `pull_request` it is the diff
between the PR base and head. See the trigger note above for why that
distinction matters.

## Docs-only gate

`main` is protected by a repository ruleset ("Protect main") that
requires two status checks: **`build`** and **`test`**. These names are
the *job ids* — `build` from `linux.yml` / `macos.yml` / `windows.yml`,
`test` from `test.yml`.

This collides with `paths-ignore`. When a workflow is skipped by
`paths-ignore`, GitHub reports **no check run at all** — not a "skipped"
result, nothing. So on a docs-only PR the required `build` and `test`
contexts never resolve, sit at "Expected — waiting for status to be
reported", and the PR is permanently blocked from merging.

`docs-gate.yml` fixes this. It has the **inverse** trigger — it runs
*only* on the paths the build/test workflows ignore — with two no-op jobs
named `build` and `test`. On a docs-only change those jobs report the
required contexts in a couple of seconds; the real workflows stay
skipped.

On a PR that touches both docs and code, the gate *and* the real
workflows both run, producing two check runs per required name. A
required check needs every run of that name to pass, so the real build
still gates the merge — the no-op gate run cannot mask a real failure.

When you change the shared `paths-ignore` set, change `docs-gate.yml`'s
`paths:` list to match. They are inverses of the same set and drift
between them reopens the blocked-PR hole.

## Documentation links

`docs-links.yml` runs [scripts/ci/check-docs-links.sh](../scripts/ci/check-docs-links.sh),
which covers two breakage classes that need two different tools:

- **Markdown links and `#anchors`,** via `lychee` over tracked `.md`.
- **`docs/<name>.md` paths named by bare path** in source comments, tests,
  scripts and CMake. That is not link syntax, so a link checker cannot see it,
  and moving a doc breaks those references silently. Markdown is excluded from
  this half on purpose: prose names docs that do not exist yet and paths
  belonging to other repositories, and both would read as failures. A
  placeholder path in a non-markdown file reads as a real reference, so name a
  doc that exists when writing a usage example in a comment or a config.

The same job then runs [scripts/ci/check-docs-symbols.py](../scripts/ci/check-docs-symbols.py),
which covers a third class the first two cannot: a link that still resolves to a
file the code has left. When a function moves the path stays valid and the prose
keeps naming the old file, so both checks above stay green while the doc sends a
reader to grep a file that no longer has it.

It reads the shapes a doc uses to make that claim. `` `Foo()` in [FILE] `` says
where Foo lives, so Foo must be *defined* there; a mention is not enough, since
the call site usually stays behind when a function moves. `` `Foo` ([FILE]) ``
and file-map table rows point at a place Foo appears, so a mention satisfies
them. Shared globals and header-only macros, types and constants are skipped,
being read everywhere by design. Anything looser was measured and rejected:
treating every symbol on a line with a link as a claim reported 38 references,
nearly all of them true and meaningless.

Run either half locally with `make docs-links` / `make docs-symbols`. The link
half needs `lychee` on PATH and **skips silently without it**, still exiting 0,
so check its `Total ... Errors` summary line actually printed.

All three checks read tracked files only, so a file you have just created is
invisible to them until it is staged. Run them after `git add`, not only after
editing, or CI will see references your local run could not.

lychee's own settings live in [lychee.toml](../lychee.toml) at the repo root,
which lychee discovers on its own. The script passes only the flags that should
differ between a terminal and a CI log, so a bare `lychee README.md` applies the
same rules CI does. Excludes and per-host throttles belong in that file, where
both paths pick them up.

Two deliberate choices:

- **No path filter.** A reference breaks from either side: renaming a doc
  breaks the source comments that point at it, and the build workflows skip
  docs paths while `docs-gate.yml` skips everything else. Neither trigger alone
  covers it, and the check costs a few seconds.
- **External URLs are weekly, not per-PR.** Not for cost: the tree holds
  roughly 350 unique external URLs, 332 of them `github.com`, and a full
  unauthenticated pass takes about 30 seconds without drawing a single 429,
  because lychee caps concurrent requests per host and spaces them out. The
  reason is that a third-party host being down would fail a PR that never
  touched it, and people would learn to ignore a red check. A rotted URL is
  still worth knowing about, which is what the Monday run is for.

## Caching and upstream dependencies

Nothing in this repo is vendored: every run rebuilds its toolchain from
somewhere else on the internet. That makes third-party availability the
single largest source of red CI, ahead of actual test failures. The
failures in the run history are all upstream, and they come in two
shapes:

- **An error comes back.** `curl` of the UASM release zip returned 503,
  failing the ASM suite before a single test compiled.
  `appimagetool`'s download exhausted its own five internal retries,
  failing an AppImage release leg.
- **Nothing comes back.** `azure.archive.ubuntu.com` `Ign:`'d every line,
  apt fell back to the public archive, and `apt-get update` then went
  silent for 29 minutes until the job timeout cancelled the leg. This
  shape is the nastier one: retry counts do not see it, because there is
  no error to retry on.

Four rules follow from that, and the workflows apply them:

1. **Bake, don't fetch.** Anything that can live in a cached image or a
   cached prefix goes there. UASM is unpacked in `docker/Dockerfile.test`
   rather than downloaded on each `docker run`; SDL3 for Android is built
   once per `(ABI, tag, NDK)` and restored from the Actions cache after.
2. **Retry what must be fetched.** `curl --retry 5 --retry-all-errors`,
   `apt-get -o Acquire::Retries=5`, and a three-attempt loop around the
   whole AppImage script (which fetches from Arch mirrors and a GitHub
   release through tooling we do not control).
3. **Put a wall clock on it as well.** A retry count only covers the
   first shape above, so a fetch that can stall also wants a `timeout`
   and per-connection limits to fail in bounded time with a named cause.
   See [apt is bounded, not just
   retried](#apt-is-bounded-not-just-retried). This is applied to the apt
   phases in `linux.yml`, `reusable-build-linux-tarball.yml` and
   `.github/actions/setup-sdl3`, and to the SDL3 clones. It is *not* yet
   applied to `pacman -Syu` and the appimagetool download in
   `scripts/packaging/make-appimage.sh` (three attempts, no wall clock),
   the apt in `docker/Dockerfile.test` (neither), or the UASM `curl`
   there (`--retry` but no `--max-time`). Those keep the exposure this
   section describes; add the bound when you next touch one.
4. **Bound every job.** All jobs carry `timeout-minutes`, so nothing can
   burn the six-hour default. Treat this as the backstop, not the bound:
   when it is what stops a job, the log ends mid-step with no diagnosis,
   which is exactly the 29-minute case above.

### What is cached

| Cache | Owner | Key | Restores |
|---|---|---|---|
| SDL3 build | `.github/actions/setup-sdl3` | pinned SDL tag + `runner.os` + `runner.arch` + link mode + build type | ~2 MB prefix, seconds |
| MSYS2 install + pacman packages | `msys2/setup-msys2` (`cache: true` default) | package list + config hash | ~177 MB |
| ASM test image layers | `docker/build-push-action`, `type=gha` | `docker/Dockerfile.test` content + the `SDL3_VERSION` build arg | the whole image, ~300 MB; written by `main` only |
| SDL3 for Android | `actions/cache` | ABI + hash of the workflow and `.github/sdl3-version.txt` | source tree + install prefix |

The ASM test image is the one that matters most. `run_tests_docker.sh`
builds it whenever it is not already in the local daemon, which on a
fresh runner is *every* run: apt, two SDL3 source builds, and the UASM
download, about 2m10s of a 2m45s job. `test.yml` builds it through buildx
with the Actions cache backend first, so `run_tests_docker.sh` finds the
image already loaded and skips its own build. Measured: 24s to restore
against 131s to build, taking the job from 165s to 67s. That step is
`continue-on-error`: if buildx or the cache service is unavailable,
nothing is loaded and `run_tests_docker.sh` builds the image itself, so
the job is slower but not red.

Only `main` writes that cache. Actions cache scopes are per-ref, and a
`pull_request` run reads its own scope plus the base branch, never a
sibling topic branch. A topic branch exporting the image therefore writes
a ~300 MB copy no other run can restore, and pays about 80s for the
export on exactly the runs that missed. Read-only elsewhere keeps a hit at 24s
and a miss at plain build cost. The practical consequence: a PR that
edits `docker/Dockerfile.test` rebuilds the image on every run until it
merges, because nothing on `main` matches its key yet.

Note that the image's 64-bit SDL3 install is unused by CI: the
`linux_test` preset is `-m32` throughout and points `SDL3_DIR` at
`/usr/local/sdl3-32`. It is kept for interactive use of the image. With
the layers cached its build cost is only paid when the Dockerfile
changes.

**ccache is deliberately absent.** The compile steps are 20s (Linux), 32s
(macOS) and 56s (Windows). Only Windows would be worth attacking, and a
ccache directory that churns on every run would compete with the image
layers for the repository's 10 GB Actions cache budget. Revisit if the
Windows build grows or the cache budget frees up.

### Where the time goes

Compilation is not the bill. Measured on a warm push, with every cache
hitting, per workflow (they run in parallel, so the gate is the slowest
one, not the sum):

| Workflow | Wall clock | Largest step | What that step is |
|---|---|---|---|
| Windows | 146s | Setup MSYS2, 65s | package restore |
| Linux | 90s | Validate packaging metadata, 45s | `apt-get install appstream` |
| ASM Docker tests | 57-73s | image restore, 16-30s | Docker layer cache |
| macOS | 57s | Build, 32s | compilation |
| Format | 43s | check-format, 18s | clang-format |
| Lint | 23s | | |

So on three of the four build workflows the biggest single step is
acquiring dependencies, not using them. That is the pattern to check
first whenever a leg feels slow: read the step timings before assuming
the build got heavier.

Two things are worth attacking if CI time becomes a problem again, in
this order. Neither is urgent, and neither has been done.

1. **`Validate packaging metadata` in `linux.yml`, 45s.** It apt-installs
   `desktop-file-utils` and `appstream` so two validators can spend about
   a second checking two generated files, and it does this on every push
   including the overwhelming majority that touch nothing in `packaging/`.
   Gating it on its inputs would take the Linux workflow to roughly 45s,
   at which point `build-clang` becomes the critical path and there is
   nothing left there worth cutting. The catch to handle: the files are
   generated from CMake variables (`LBA2_DESKTOP_ID` and friends), so any
   trigger has to cover the CMake that templates them, and a
   validation-only job still needs a configured build tree.
2. **`Setup MSYS2` on Windows, 65s.** The largest non-compile step in the
   gate once the above is gone. Nobody has measured whether that is the
   ~177 MB cache restore or the pacman work after it, and the answer
   decides whether there is anything to do.

The ASM Docker image is already handled and is not on this list: it looks
expensive on a PR that edits `docker/Dockerfile.test`, because only `main`
writes that cache scope, so such a PR rebuilds on every run until it
merges.

### One SDL3 pin

`.github/sdl3-version.txt` holds the SDL3 tag, and every path that
acquires SDL3 from source reads it: `.github/actions/setup-sdl3` (used by
`linux.yml`, `macos.yml`, `reusable-build-linux-tarball.yml` and
`reusable-build-macos.yml`), `reusable-build-android.yml`,
`docker/Dockerfile.test` via a `SDL3_VERSION` build arg, and the
`scripts/dev/` helpers. Bumping SDL3 is an edit to that one file, and it
invalidates every SDL3 cache key in the repo on its own.

Two paths stay outside it, because their SDL3 is a distro package rather
than a source build: MSYS2's `mingw-w64-ucrt-x86_64-sdl3` on the Windows
legs, and Arch's `sdl3` inside the AppImage container. Those track what
their distro ships. Consolidating them would mean building SDL3 from
source under MSYS2 and inside the container, which costs more than the
drift does.

### apt is bounded, not just retried

Every `apt-get` in the Linux workflows runs under a wall clock and a
retry loop, not `Acquire::Retries` alone. `Retries` only re-tries a
request that came back with an error; a mirror that accepts the
connection and then stalls produces no error and apt has no default
timeout of its own. Observed on a cold `linux.yml` leg:
`azure.archive.ubuntu.com` returned `Ign:` on every line, apt fell back
to `archive.ubuntu.com`, and `apt-get update` then went silent for 29
minutes until the job timeout cancelled it.

So each phase carries `Acquire::http::Timeout`/`https::Timeout=15`, a
`timeout -k 30` wall clock (120s for `update`, 300s for `install`), and
three attempts. A mirror that is down for good now fails the leg in about
21 minutes with a named cause rather than going quiet until the timeout.
For scale, a healthy runner does `update` in ~5s. The job timeouts on
`linux.yml` and `macos.yml` are 45 minutes, matching the release legs:
30 was sized for a world where SDL3 was always a cache hit, and a genuine
cold build has to fit too.

`.github/actions/setup-sdl3` owns its cache rather than delegating to
`libsdl-org/setup-sdl`. The reason is in the action's own header comment:
setup-sdl's `install-linux-dependencies` runs before it knows whether the
cache hit, so the Linux legs paid an apt-get of 86 dev packages on every
run for a source build that never happened. Median 64s, worst case 19
minutes, against a game build of 11-27s. Owning the cache is what makes
that install conditional. It also puts `runner.arch` in the key, which is
what the tarball workflow previously needed a `discriminator` for.

### Where the toolchain still floats

`msys2/setup-msys2` runs with `update: true`, so the MinGW toolchain
advances whenever MSYS2 publishes. That is deliberate, in that it
surfaces upstream breakage early rather than letting it accumulate, but
it is the reason a green Windows run yesterday is not proof of a green
one today.

### Action pinning

Third-party actions use a mutable major tag (`@v2`…`@v7`), and the
container image for the AppImage build is
`ghcr.io/pkgforge-dev/archlinux:latest`. That is the usual trade-off
between supply-chain exposure and maintenance load; if it is ever
tightened, do it with Dependabot's `github-actions` ecosystem so the pins
have something keeping them current.

## Release tier

The release workflows are documented in full in
[RELEASING.md](RELEASING.md) — see "Release workflow conventions" and
"Adding a new release target". In short: each `release-<platform>.yml`
is a thin caller that delegates its build to a
`reusable-build-<platform>.yml` (`workflow_call`) workflow, then attaches
the artifact to a GitHub Release; `release-latest.yml` calls the same
reusables to maintain a rolling `latest` pre-release. Release builds
static-link and are tag- or `main`-triggered, so they sit outside the
per-push validation path.

## Branch protection

The "Protect main" ruleset requires the `build` and `test` checks to pass
before a PR can merge. It does **not** require `check-format` or the
PR-title lint — those run and are visible on the PR, but are advisory.
If you rename the `build` or `test` job ids, update the ruleset's
required-check list and `docs-gate.yml` to match, or every PR will block.
