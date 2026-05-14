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
| `linux.yml` | Validation | push, PR, dispatch | `build` | Configure (`linux` preset), build `lba2cc` + `host_tests`, run `ctest -L host_quick` |
| `macos.yml` | Validation | push, PR, dispatch | `build` | Same, on `macos-latest` (`macos_arm64` preset) |
| `windows.yml` | Validation | push, PR, dispatch | `build` | Same, on Windows MSYS2 UCRT64 (`windows_ucrt64` preset) |
| `test.yml` | Validation | push, PR, dispatch | `test` | Docker: `./run_tests_docker.sh` — full ASM↔C++ equivalence suite (Linux only, slow) |
| `format.yml` | Validation | push, PR, dispatch | `check-format` | `scripts/ci/check-format.sh` (clang-format) |
| `pr-title.yml` | Validation | PR | `lint` | Conventional-commit lint on the PR title |
| `docs-gate.yml` | Validation | push, PR | `build`, `test` | No-op stand-ins for the required `build`/`test` checks on docs-only changes — see [below](#docs-only-gate) |
| `release-*.yml` | Release | `v*` tags, dispatch | `build` → `release` | Per-platform tag releases |
| `reusable-build-*.yml` | Release | `workflow_call` | `build` | Shared build+package steps, called by the release workflows |
| `release-latest.yml` | Release | push to `main` | build legs → `release` | Rolling `latest` pre-release |

Host build jobs (`linux`/`macos`/`windows`) need neither retail game
files nor Docker. The Docker job (`test.yml`) builds a 32-bit UASM image
and replays polyrec captures; it does not run the host discovery tests.

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
