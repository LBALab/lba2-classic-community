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

Prerequisites: [`git-cliff`](https://git-cliff.org/) installed locally.

```bash
# 1. Pick the version. Pre-1.0 examples: v0.9.0, v0.9.1, v0.10.0
VERSION=v0.9.0

# 2. Regenerate CHANGELOG.md from history. The [Unreleased] section
#    becomes the new version's section. GITHUB_TOKEN lets cliff resolve
#    each commit's GitHub username (`@handle`) for per-line attribution.
#    Without it the template falls back to git author names — still works,
#    just less linkable. Use a token with public read scope only.
GITHUB_TOKEN="$(gh auth token)" git cliff --tag "$VERSION" -o CHANGELOG.md

# 3. Review the diff. Hand-curate the top of the new section if useful
#    (group highlights, link to docs).
git diff CHANGELOG.md

# 4. Commit and tag.
git add CHANGELOG.md
git commit -m "chore(release): $VERSION"
git tag -a "$VERSION" -m "Release $VERSION"

# 5. Push.
git push origin main
git push origin "$VERSION"
```

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
- The PR title drives each changelog entry. Whichever merge mode you pick
  (merge commit, squash, or rebase), the PR title ends up on `main` as a
  conventional-commit-formatted line that cliff parses. The PR-title CI
  check (`.github/workflows/pr-title.yml`) is what keeps the log clean
  enough.
- **Optional GitHub setting:** Settings → General → "Default to PR title
  for merge commits". Turning this on makes merge-commit subjects read as
  the clean PR title (e.g. `fix(credits): ... (#66)`) instead of the noisy
  `Merge pull request #66 from ...` default. Highly recommended if the
  repo uses merge commits as a default — it lets cliff treat them as
  normal entries.
- **`chore:` is the explicit "skip from changelog" prefix.** Use it for
  housekeeping commits (formatting, internal refactors that aren't worth
  surfacing to readers, tooling fixes). Cliff drops them entirely.
- The `[remote.github]` block in `cliff.toml` enables author handle
  lookup via the GitHub API (`commit.remote.username`). Each entry then
  ends with ` — by @handle` linking to the author's profile. If the API
  call is unauthenticated and rate-limited, cliff falls back to the git
  author name — exporting `GITHUB_TOKEN` (above) avoids that.
