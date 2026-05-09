# Releasing

Short maintainer recipe for cutting a release.

## Versioning

The engine uses [Semantic Versioning](https://semver.org/). The fork is
pre-1.0 while crashes are still being ironed out.

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

These are independent:

- **Engine version** (`v0.9.0`, `v1.0.0`, ...) — the semver tag this doc is about.
- **`NUM_VERSION`** in `SOURCES/COMMON.H` — the save-file on-disk format
  version (currently `36`). Bumping the engine version does not bump
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
canonical content for `v0.9.0`. Do not run `git cliff -o` here; it
would replace the narrative with a sparse auto-generated section (most
pre-tag commits are not conventional-commit-formatted). Just promote the
heading.

The version number itself lives in the `VERSION` file at the repo
root. CMake reads it, embeds it in the binary, and writes it to
`build/VERSION.txt` for release pipelines (PR #74) to consume. So a release
cut also bumps that file: `0.9.0-dev` → `0.9.0` for the release commit,
then `0.10.0-dev` for the next development cycle.

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

# 2. Bump VERSION file: 0.9.0-dev -> 0.9.0
echo "0.9.0" > VERSION

# 3. Review, commit, tag, push.
git diff CHANGELOG.md VERSION
git add CHANGELOG.md VERSION
git commit -m "chore(release): $VERSION"
git tag -a "$VERSION" -m "Release $VERSION"
git push origin main
git push origin "$VERSION"

# 4. Sanity-check the binary picks it up.
cmake --build build
./build/SOURCES/lba2cc --version    # expect: 0.9.0
cat build/VERSION.txt                  # expect: 0.9.0

# 5. Bump VERSION for next development cycle: 0.9.0 -> 0.10.0-dev
echo "0.10.0-dev" > VERSION
git add VERSION
git commit -m "chore: bump VERSION to 0.10.0-dev"
git push origin main
```

`git-cliff` is not required for the first release.

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

**Why `GITHUB_TOKEN` is required.** The template renders one entry per
PR using `commit.remote.pr_title` and `commit.remote.pr_number`, which
cliff fetches from the GitHub API for each commit. This is what gives a
single clean entry per PR regardless of merge mode (squash, merge-commit,
or rebase). Without a token, those fields are empty and no entries
render. Use a token with public read scope only.

## GitHub release

A git tag is just a pointer to a commit. A GitHub Release is a
separate object layered *on top of* a tag — it adds the user-facing
Releases-page entry (`/releases/tag/v0.9.0`), a title and rendered
release notes, optional binary attachments, and an RSS feed (`/releases.atom`)
that auto-update tools watch. Pushing the tag without creating a Release
means it shows up under "Tags" but not "Releases" — most browsers and
tools only look at the latter.

### Drafting the release

Two equivalent paths.

**CLI (recommended for repeatability):**

```bash
# Extract the v0.9.0 section from CHANGELOG.md into a notes file
sed -n '/^## \[0\.9\.0\]/,/^## \[/{/^## \[/!p;}' CHANGELOG.md > /tmp/release-notes.md

# Create the GitHub Release from the existing tag
gh release create v0.9.0 --title "v0.9.0" --notes-file /tmp/release-notes.md
```

**Web UI:** Releases → Draft a new release → pick the `v0.9.0` tag →
paste the v0.9.0 section from `CHANGELOG.md` as the body → Publish.

For v0.9.0 there are no binary attachments yet. The release-binary
workflow is tracked in
[issue #46](https://github.com/LBALab/lba2-classic-community/issues/46)
and [PR #74](https://github.com/LBALab/lba2-classic-community/pull/74)
(Linux AppImages). Source-only releases are valid; players build from
source per the [README](../README.md).

For future releases, `gh release create --generate-notes` auto-fills
the body from PR titles since the previous tag — useful as a sanity
check against the cliff-generated CHANGELOG section.

## After the release

Tell people. A short Discord post in the LBALab community channel
helps the release land:

```
v0.9.0 is out — first tagged release of the fork.

Changelog: https://github.com/LBALab/lba2-classic-community/blob/main/CHANGELOG.md
Release: https://github.com/LBALab/lba2-classic-community/releases/tag/v0.9.0

No binaries yet — that's coming via #74 (AppImages) and follow-ups for
macOS/Windows. For now, build from source per the README.
```

### Natural follow-ups

These become unblocked once `v0.9.0` is tagged:

- **Linux AppImages**
  ([PR #74](https://github.com/LBALab/lba2-classic-community/pull/74))
  and macOS/Windows release pipelines. They consume `build/VERSION.txt`
  to name artifacts.
- **Optional GitHub repo setting:** Settings → General → "Default to
  PR title for merge commits". Turning it on makes merge-commit
  subjects in the git log match the PR title. Cosmetic — cliff handles
  both formats already.

## How the binary knows its version

The `VERSION` file at the repo root is the single source of truth.
CMake reads it at build time and produces:

- **`build/VERSION.txt`** — plain text, the resolved version. Release
  pipelines (PR #74) `cat` this to name artifacts.
- **`LBA2_VERSION_STRING` macro** in `build/VERSION_GENERATED.h`,
  pulled in via `BUILD_INFO.h`. The game banner (`Version` in
  `SOURCES/VERSION.CPP`) and `./lba2cc --version` both read it.

If git is available and the working tree has uncommitted changes, the
resolved version is suffixed with `-dirty`. So:

| Build context | `./lba2cc --version` |
|---|---|
| Tarball (no `.git`) | `0.9.0-dev` (whatever the file says) |
| Clean dev clone | `0.9.0-dev` |
| Dev clone with uncommitted changes | `0.9.0-dev-dirty` |
| At a release commit (file says `0.9.0`) | `0.9.0` |

The maintainer bumps the `VERSION` file as part of cutting a release
(see step 2 in the recipe above). After the bump-and-commit lands on
`main`, `./lba2cc --version` reports the real semver.

## Product metadata overrides

Release-facing strings (executable name, runtime window title,
`.desktop` entry, AppImage label) all flow from one place: the `LBA2_*`
cache variables in the root `CMakeLists.txt`. Override any of them at
configure time and every surface follows.

| Cache variable             | Default                         | Used by                                                    |
|----------------------------|---------------------------------|------------------------------------------------------------|
| `LBA2_EXECUTABLE_NAME`     | `lba2cc`                        | binary name, `.desktop` `Exec=` / `StartupWMClass`         |
| `LBA2_PRODUCT_NAME`        | `LBA2 Classic Community`        | window title (with version), `.desktop` `Name`, AppImage   |
| `LBA2_PRODUCT_NAME_DEMO`   | `LBA2 Twinsen's Odyssey Demo`   | window title when built with `-DDEMO`                      |
| `LBA2_PRODUCT_DESCRIPTION` | (one-line fork description)     | `.desktop` `Comment`                                       |
| `LBA2_DESKTOP_ID`          | `lba2cc`                        | `.desktop` filename stem and `Icon=` value                 |

Example — produce an alternate-branded build:

```bash
cmake -B build \
      -DLBA2_PRODUCT_NAME="LBA2 Anniversary Build" \
      -DLBA2_EXECUTABLE_NAME=lba2-anniv
cmake --build build
```

The window title bar reads `LBA2 Anniversary Build <version>`, the
binary lands at `build/SOURCES/lba2-anniv`, and the generated
`build/packaging/lba2cc.desktop` carries the matching `Name=` and
`Exec=` entries. The AppImage script (`packaging/make-appimage.sh`)
sources `build/packaging/appimage_env.sh` so its outputs follow too.

> **Note on `LBA2_DESKTOP_ID`.** The icon PNG is a committed asset at
> `packaging/lba2cc.png`. If you override `LBA2_DESKTOP_ID` (say to
> `lba2-anniv`), the AppImage script will look for
> `packaging/lba2-anniv.png` and fail unless you've placed a matching
> file. The binary, window title, and `.desktop` entry don't need the
> icon and follow overrides cleanly on their own.

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
