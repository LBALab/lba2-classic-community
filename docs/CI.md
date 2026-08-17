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
| `linux.yml` | Validation | push, PR, dispatch | `build`, `build-clang`, `build-demo`, `sanitize` | Configure (`linux` preset), build `lba2cc` + `host_tests`, run `ctest -L host_quick`; the same through clang and for the demo SKU; and the host tests once more under Address + UndefinedBehavior sanitizers |
| `macos.yml` | Validation | push, PR, dispatch | `build` | Same, on `macos-latest` (`macos_arm64` preset) |
| `windows.yml` | Validation | push, PR, dispatch | `build` | Same, on Windows MSYS2 UCRT64 (`windows_ucrt64` preset) |
| `test.yml` | Validation | push, PR, dispatch | `test` | Docker: `./run_tests_docker.sh` — full ASM↔C++ equivalence suite (Linux only, slow) |
| `format.yml` | Validation | push, PR, dispatch | `check-format` | `scripts/ci/check-format.sh` (clang-format) |
| `lint.yml` | Validation | push, PR (shell/Python/workflow paths), dispatch | `shellcheck`, `actionlint`, `ruff` | Lints the non-C++ surface. Settings live in `.shellcheckrc` and `ruff.toml`; tool versions are pinned in the workflow |
| `pr-title.yml` | Validation | PR | `lint` | Conventional-commit lint on the PR title |
| `docs-gate.yml` | Validation | push, PR | `build`, `test` | No-op stand-ins for the required `build`/`test` checks on docs-only changes — see [below](#docs-only-gate) |
| `docs-links.yml` | Validation | push, PR, weekly | `links`, `external` | `scripts/ci/check-docs-links.sh`. Not path-filtered: a doc reference breaks from either side, since renaming a doc breaks source comments. `external` is weekly-only; see [below](#documentation-links) |
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
  belonging to other repositories, and both would read as failures.

Two deliberate choices:

- **No path filter.** A reference breaks from either side: renaming a doc
  breaks the source comments that point at it, and the build workflows skip
  docs paths while `docs-gate.yml` skips everything else. Neither trigger alone
  covers it, and the check costs a few seconds.
- **External URLs are weekly, not per-PR.** The tree holds roughly 580 external
  URLs, 446 of them `github.com`. Unauthenticated checking trips GitHub's rate
  limit well before it finishes, so a per-PR run would fail for reasons that
  have nothing to do with the PR, and people would learn to ignore it. A rotted
  URL is still worth knowing about, which is what the Monday run is for.

Run it locally with `make docs-links`. Without `lychee` installed the link half
is skipped with a notice and the path half still runs.

## Caching and upstream dependencies

Nothing in this repo is vendored: every run rebuilds its toolchain from
somewhere else on the internet. That makes third-party availability the
single largest source of red CI, ahead of actual test failures. Both
failures in the recent run history were upstream 5xx responses, not code:

- `curl` of the UASM release zip returned 503, failing the ASM suite
  before a single test compiled.
- `appimagetool`'s download exhausted its own five internal retries,
  failing an AppImage release leg.

Three rules follow from that, and the workflows apply them:

1. **Bake, don't fetch.** Anything that can live in a cached image or a
   cached prefix goes there. UASM is unpacked in `docker/Dockerfile.test`
   rather than downloaded on each `docker run`; SDL3 for Android is built
   once per `(ABI, tag, NDK)` and restored from the Actions cache after.
2. **Retry what must be fetched.** `curl --retry 5 --retry-all-errors`,
   `apt-get -o Acquire::Retries=5`, and a three-attempt loop around the
   whole AppImage script (which fetches from Arch mirrors and a GitHub
   release through tooling we do not control).
3. **Bound every job.** All jobs carry `timeout-minutes`. Without it a
   hung mirror connection burns the six-hour default before the job is
   marked failed.

### What is cached

| Cache | Owner | Key | Restores |
|---|---|---|---|
| SDL3 build | `libsdl-org/setup-sdl` (built in) | resolved SDL git hash + `runner.os`/`arch` | ~2 MB prefix, seconds |
| MSYS2 install + pacman packages | `msys2/setup-msys2` (`cache: true` default) | package list + config hash | ~177 MB |
| ASM test image layers | `docker/build-push-action`, `type=gha` | `docker/Dockerfile.test` content | the whole image, ~300 MB; written by `main` only |
| SDL3 for Android | `actions/cache` | ABI + SDL tag + NDK version | source tree + install prefix |

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

**ccache is deliberately absent.** The compile steps are 14 s (macOS),
18 s (Linux) and 70 s (Windows); only Windows is worth attacking, and a
ccache directory that churns on every run would compete with the image
layers for the repository's 10 GB Actions cache budget. Revisit if the
Windows build grows or the cache budget frees up.

### Where the toolchain still floats

Two versions are resolved at run time rather than pinned:

- `libsdl-org/setup-sdl` is called with `version: 3-latest` in
  `linux.yml`, `macos.yml`, `reusable-build-linux-tarball.yml`, and
  `reusable-build-macos.yml`, while `docker/Dockerfile.test`,
  `reusable-build-android.yml`, and the `scripts/dev/` helpers pin
  `release-3.2.16`. A new SDL3 release therefore invalidates the setup-sdl
  cache and changes what CI links against with no commit in this repo.
- `msys2/setup-msys2` runs with `update: true`, so the MinGW toolchain
  advances whenever MSYS2 publishes.

Both are deliberate, in that they surface upstream breakage early rather
than letting it accumulate, but they are the reason a green run yesterday
is not proof of a green run today.

### Action pinning

`libsdl-org/setup-sdl` is pinned to a commit SHA. Everything else uses a
mutable major tag (`@v2`…`@v7`), and the container image for the AppImage
build is `ghcr.io/pkgforge-dev/archlinux:latest`. That is the usual
trade-off between supply-chain exposure and maintenance load; if it is
ever tightened, do it with Dependabot's `github-actions` ecosystem so the
pins have something keeping them current.

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
