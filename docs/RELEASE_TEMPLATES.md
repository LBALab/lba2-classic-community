# Release templates

Fillable templates and a fact-check checklist for cutting a release. Sits
alongside [docs/RELEASING.md](RELEASING.md) — RELEASING is the workflow,
this is the content.

Three surfaces carry overlapping prose at release time and used to drift
because each was written from scratch:

1. **`CHANGELOG.md`** — canonical narrative, hard-wrapped, lives forever.
2. **GitHub Release body** (`/releases/tag/vX.Y.Z`) — extracted from
   the CHANGELOG section, single-line-per-bullet so non-GitHub renderers
   (RSS, `gh release view`, email) stay readable.
3. **GitHub Discussions announcement** (under **Announcements**) — a
   shorter, casual restatement aimed at players not poring over a
   changelog.

Pattern: write CHANGELOG once, extract / re-render the other two from
it. Don't compose any of the three from scratch.

The Discord post is shorter still and rendered from the same content;
template at the bottom of this doc.

## Pre-tag fact-check checklist

Five minutes per release; the v0.10.0 retro caught ten discrepancies
post-publish that this would have surfaced before publishing.

For each bullet under `## [Unreleased]` in `CHANGELOG.md`:

- [ ] Cross-check the headline claim against the originating PR body
      (`gh pr view <N> --json body`). Watch for: PR-body verbs that don't
      match the bullet ("compile fix" vs the bullet's "runtime
      corruption"), platforms the PR didn't actually touch, and feature
      claims the PR explicitly scoped out.
- [ ] If the bullet quotes user-facing strings (filename formats, error
      text, config keys), grep the code to confirm they match.
- [ ] If the bullet mentions a doc file as **new**, confirm with
      `git cat-file -e v<PREV_TAG>:docs/<FILE>` that it didn't already
      exist at the previous tag. "Extended" is a separate verb from
      "added".
- [ ] If the bullet quotes a phase number from a roadmap doc
      (`docs/WIDESCREEN.md`, etc.), open the doc and confirm the phase
      letter / number.
- [ ] Spot-check the time window in any intro narrative. v0.10.0's "six
      weeks after v0.9.0" was actually eight days — easy to write,
      embarrassing once published.

## Release body wrapper

The Release body is the `## [vX.Y.Z]` section of `CHANGELOG.md`
extracted via `awk`, wrapped in a short header paragraph and a Full
Changelog footer, then un-wrapped from 72-col hard-wrap to
single-line-per-bullet so it reads in CLI / RSS / email renderers
without horizontal scroll glyphs.

Extraction:

```bash
VERSION=v0.10.0
awk "/^## \\[${VERSION#v}\\]/{f=1;next} /^## \\[/{f=0} f" CHANGELOG.md \
    > /tmp/${VERSION}-section.md
```

Wrapper (head + extracted CHANGELOG + foot):

```markdown
<one-paragraph headline>. <one-sentence about ~N merged PRs since v(X.Y-1).0>: <comma-separated list of the four or five things this release is about>.

<one-sentence new-contributor welcome, or omit>.

Binaries for <platform list> attached below. <Smoke-test result if you ran one>. <Platform-specific caveats — e.g. macOS Gatekeeper bypass>.

---

<extracted CHANGELOG section>

---

**Full Changelog:** https://github.com/LBALab/lba2-classic-community/compare/v<PREV>...v<NEW>
```

Un-wrap (markdown collapses single newlines within a paragraph; this
step keeps the on-page render identical and makes the raw text
scannable):

```python
import re, pathlib
src = pathlib.Path(infile).read_text().replace("\r\n", "\n")
out, buf = [], ""
def flush():
    global buf
    if buf: out.append(buf); buf = ""
for line in src.split("\n"):
    s = line.rstrip()
    if s == "":
        flush(); out.append(""); continue
    if re.match(r"^(- |#{1,6} |> |---$)", s):
        flush(); buf = s
    else:
        buf = s if buf == "" else buf + " " + s.lstrip()
flush()
print(re.sub(r"\n{3,}", "\n\n", "\n".join(out)))
```

Publish:

```bash
gh release edit $VERSION --notes-file /tmp/${VERSION}-body.md
```

Worked example: the v0.10.0 release body —
<https://github.com/LBALab/lba2-classic-community/releases/tag/v0.10.0>.

## GitHub Discussion announcement

Posted under **Announcements** via the discussion creation flow. Shape:

```markdown
<one-paragraph hook framing the release — what's the headline and why does
it matter to a player, not a contributor>.

## What's in v<X.Y.Z>:

* <Headline feature> — <one-line plain-English description with concrete
  user-visible impact, not internal terms>.
* <Second feature> — ditto.
* <Stability / fixes one-liner> — name the most visible bug-fix in
  player terms; leave the static-analysis-pass framing for the changelog.
* <Quality-of-life / tooling> — group; don't enumerate each PR.
* <Foundation-work bullet> — phrase as "lays the groundwork for X" with
  a pointer to the relevant docs/ file.

Release: https://github.com/LBALab/lba2-classic-community/releases/tag/v<X.Y.Z>
Full changelog: https://github.com/LBALab/lba2-classic-community/blob/main/CHANGELOG.md
Discord: https://discord.gg/jsTPWYXHsh

<New-contributor welcome if any; thanks line; one-sentence forward look
to the next milestone>.
```

Worked examples:

- v0.9.0 — <https://github.com/LBALab/lba2-classic-community/discussions/121>
- v0.10.0 — <https://github.com/LBALab/lba2-classic-community/discussions/154>

Posting via the API (`gh` CLI doesn't have a `discussion create`
verb yet):

```bash
REPO_ID=$(gh api graphql -f query='
  { repository(owner: "LBALab", name: "lba2-classic-community") { id } }' \
  --jq '.data.repository.id')
CAT_ID=$(gh api graphql -f query='
  { repository(owner: "LBALab", name: "lba2-classic-community") {
      discussionCategories(first: 20) { nodes { id name } } } }' \
  --jq '.data.repository.discussionCategories.nodes
        | map(select(.name == "Announcements"))[0].id')
BODY=$(python3 -c 'import json,pathlib; print(json.dumps(pathlib.Path("/tmp/announcement.md").read_text()))')

gh api graphql -f query="
mutation {
  createDiscussion(input: {
    repositoryId: \"$REPO_ID\",
    categoryId: \"$CAT_ID\",
    title: \"v<X.Y.Z> released - <one-line hook>\",
    body: $BODY
  }) { discussion { url number } }
}"
```

To edit a posted discussion: same shape, `updateDiscussion(input: {
discussionId, body })` instead of `createDiscussion`.

## Discord post

Posted in the LBALab community Discord. ~600 chars; angle-bracket the
URLs to suppress duplicate embeds:

```
**v<X.Y.Z> is out** 🎉

<one-line hook — what's the headline of this release in plain
English>. Plus:

• **<Headline feature>** — <one-line description>.
• **<Second feature>** — ditto.
• **<Third — visible-symptom bug fix>** — name the player-visible
  symptom, not the internal cause.
• **<Group everything else>** — UB hardening / build / docs / etc.,
  rolled into one bullet.

<New-contributor welcome if any> 👋

Full announcement & discussion: <https://github.com/LBALab/lba2-classic-community/discussions/<DISCUSSION#>>
Download: <https://github.com/LBALab/lba2-classic-community/releases/tag/v<X.Y.Z>>
```

Discord-specific:

- Use `•` not `-` — Discord renders `-` as a plain dash, not a bullet.
- Angle-bracket URLs (`<https://…>`) to suppress link previews; otherwise
  two big embed cards stack under the post and crowd the channel.
  Drop the brackets on one or both URLs if you want the previews.
- 2000-char message limit; current template lands around 600, well
  under.
