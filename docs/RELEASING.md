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

`1.0.0` is reached when all of the following hold:

- Main story completable on **Linux, macOS, and Windows** without known crashes.
- Save / load round-trip stable across a full playthrough.
- Equivalence and host-discovery tests pass on all three platforms in CI.
- Memory-safety hardening (issue
  [#47](https://github.com/LBALab/lba2-classic-community/issues/47)) closed
  for the main-story code paths.

Until then, MINOR releases ship features and platform progress; PATCH
releases ship fixes.

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
#    becomes the new version's section.
git cliff --tag "$VERSION" -o CHANGELOG.md

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
- The repo uses **squash-merge**, so each PR contributes exactly one commit
  to `main`, and the PR title is that commit's message. The PR-title CI
  check (`.github/workflows/pr-title.yml`) is what keeps the log clean
  enough for cliff to parse.
