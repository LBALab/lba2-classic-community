# Releasing

Short maintainer recipe for cutting a release.

## Versioning

The engine uses [Semantic Versioning](https://semver.org/). The fork is
**pre-1.0** while crashes are still being ironed out.

| Bump  | When                                                                |
|-------|---------------------------------------------------------------------|
| MAJOR | Save-format break, hard removal of a build option, or `1.0` itself  |
| MINOR | New feature, new platform support, opt-in behavior change           |
| PATCH | Bugfix or doc-only release                                          |

### `1.0.0` bar

`1.0.0` is reached when:

- **All original features are implemented and working** — feature parity with
  the retail game, no functional gaps.
- The fork has had **at least a few rounds of community play-tests** to
  surface remaining issues against a real playthrough.
- Equivalence and host tests pass on Linux, macOS, and Windows in CI.

Until then, MINOR releases ship features and platform progress; PATCH
releases ship fixes. Don't wait for `1.0` to bump versions — there's
plenty of new work in the fork already, and incremental `0.x` releases
keep momentum.

### Engine version vs `NUM_VERSION`

These are **independent**:

- **Engine version** (`v0.9.0`, `v1.0.0`, ...) — the semver tag this doc is about.
- **`NUM_VERSION`** in `SOURCES/COMMON.H` — the save-file on-disk format
  version (currently `36`). Bumping the engine version does **not** bump
  `NUM_VERSION`, and vice versa. See
  [issue #64](https://github.com/LBALab/lba2-classic-community/issues/64) for
  the canonical save-format work tracked separately.

## Cutting a release

The flow differs slightly between the very first tagged release and every
release after it. The difference is whether `git-cliff` should generate
the section content (subsequent releases) or the existing hand-curated
`[Unreleased]` section is the content (first release).

### First release (`v0.9.0`)

The current `[Unreleased]` section in `CHANGELOG.md` is hand-curated — it
narrates everything shipped since the fork was created and is the
canonical content for `v0.9.0`. **Do not run `git cliff -o`** here; it
would replace the narrative with a sparse auto-generated section (most
pre-tag commits are not conventional-commit-formatted). Just promote the
heading.

```bash
VERSION=v0.9.0
DATE=$(date +%Y-%m-%d)

# 1. In CHANGELOG.md, change:
#      ## [Unreleased]
#    to:
#      ## [Unreleased]
#      _Nothing yet._
#
#      ## [0.9.0] - YYYY-MM-DD
#
#    Update the compare link at the bottom of the file:
#      [Unreleased]: .../compare/main...HEAD
#    becomes:
#      [Unreleased]: .../compare/v0.9.0...HEAD
#      [0.9.0]:      .../compare/<initial-commit-sha>...v0.9.0
#
# 2. Review, commit, tag, push.
git diff CHANGELOG.md
git add CHANGELOG.md
git commit -m "chore(release): $VERSION"
git tag -a "$VERSION" -m "Release $VERSION"
git push origin main
git push origin "$VERSION"
```

`git-cliff` is **not required** for the first release.

### Subsequent releases

From the second release on, `git cliff --prepend` adds a new versioned
section to the top of `CHANGELOG.md`, generated from PR titles since the
previous tag. Existing content (including the hand-curated v0.9.0
narrative) stays untouched.

```bash
VERSION=v0.10.0

# 1. Prepend the new section. NOTE: --prepend, not -o (which would
#    overwrite the entire file). GITHUB_TOKEN is required: the body
#    template renders one entry per PR using the PR title resolved from
#    the GitHub API. Without a token, no entries render.
GITHUB_TOKEN="$(gh auth token)" git cliff --tag "$VERSION" --prepend CHANGELOG.md

# 2. Review the new section at the top. Hand-curate if useful (group
#    highlights, link to docs).
git diff CHANGELOG.md

# 3. Add a compare link at the bottom of the file:
#      [v0.10.0]: .../compare/v0.9.0...v0.10.0

# 4. Commit, tag, push.
git add CHANGELOG.md
git commit -m "chore(release): $VERSION"
git tag -a "$VERSION" -m "Release $VERSION"
git push origin main
git push origin "$VERSION"
```

**Why `GITHUB_TOKEN` is required.** The template renders **one entry per
PR** using `commit.remote.pr_title` and `commit.remote.pr_number`, which
cliff fetches from the GitHub API for each commit. This is what gives a
single clean entry per PR regardless of merge mode (squash, merge-commit,
or rebase). Without a token, those fields are empty and no entries
render. Use a token with public read scope only.

## GitHub Release

Creating a GitHub Release with attached binaries is tracked separately in
[issue #46](https://github.com/LBALab/lba2-classic-community/issues/46).
Until that workflow lands, push the tag (above) and optionally draft a
plain Release on GitHub with the relevant CHANGELOG section pasted as the
body. Source-only releases are valid; players build from source per the
[README](../README.md).

## What `git-cliff` reads

- Commit messages on `main` since the previous tag.
- Conventional-commit prefixes (`feat:`, `fix:`, `port:`, `docs:`, ...) drive
  the section grouping. See `cliff.toml` and the
  [Commit & PR conventions](../AGENTS.md#commit--pr-conventions) section in
  AGENTS.md.
- One entry per PR. Cliff queries the GitHub API for each commit on `main`
  to resolve its associated PR (number + title) and renders one line per
  PR. Works identically for squash-merge, merge-commit, and rebase modes —
  the merge mode doesn't matter, the PR title does. The PR-title CI check
  (`.github/workflows/pr-title.yml`) is what keeps the log clean enough.
- A small `commit_preprocessors` rule rewrites GitHub's default merge
  subject (`Merge pull request #N from owner/branch`) into the
  conventional-commit-formatted PR title that GitHub puts on the third
  line of the body. This lets the type-based grouping (Added / Fixed /
  ...) work correctly for merge-commit-mode PRs.
- **`chore:` is the explicit "skip from changelog" prefix.** Use it for
  housekeeping PRs (formatting, internal refactors that aren't worth
  surfacing to readers, tooling fixes). Cliff drops them entirely.
- The `[remote.github]` block in `cliff.toml` enables author handle
  lookup via the GitHub API (`commit.remote.username`). Each entry then
  ends with ` — by @handle` linking to the author's profile. If the API
  call is unauthenticated and rate-limited, cliff falls back to the git
  author name — exporting `GITHUB_TOKEN` (above) avoids that.
