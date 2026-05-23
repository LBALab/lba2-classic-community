# Cheap-test findings — art treatment preview

What the `scripts/dev/art_treatment_preview.py` script surfaced when run
against retail `SCREEN.HQR` with the triage from
[art-strategy.md](art-strategy.md). Findings are the point of the cheap
test — the strategy was a best guess from thumbnails, the previews are
what 853-wide actually renders.

## Reproduction

```bash
scripts/dev/art_treatment_preview.py /path/to/retail/SCREEN.HQR \
    --all --outdir docs/widescreen/catalog-dumps/preview/
```

The PNG dumps are gitignored (same copyright concern as the catalog
dumps). To compare treatments for a single entry:

```bash
scripts/dev/art_treatment_preview.py /path/to/SCREEN.HQR --entry 4 \
    --compare --out /tmp/compare_menu.png
```

## Revisions to the triage

Two rounds of revision after visual review of the `--all` output. The
script's embedded `TRIAGE` table reflects the latest state;
[art-strategy.md](art-strategy.md) was updated to match. The first round
demoted two `edge-clone` calls; the second round simplified the whole
table to letterbox-by-default after a broader look at what reads well at
853-wide.

### Round 2 — letterbox by default

After running `--all` and reviewing the full set, the verdict is that
`letterbox` is the right default for almost every entry. The exceptions:

- **Entries 0, 72, 76** (`PCR_LOGO`, `PCR_ACTIVISION`, `PCR_VIRGIN`) —
  pure-white backgrounds. `palette-fill` auto-detects white and the fill
  is genuinely invisible; the logos look unaltered. Keep `palette-fill`.
- **Entry 4** (`PCR_MENU`) — open question. The stormy sea/sky doesn't
  letterbox well (the harsh black bars contrast badly with the dark-grey
  sea). Two candidate treatments are tracked: `stretch` (resample the
  bitmap horizontally to 853, accepts ~33% distortion) and `gradient`
  (replace the bitmap entirely with a procedural blue gradient). Both
  are implemented in the script for review.

Everything else — including the entries that previously held up as
`edge-clone` (paper-on-wood maps, bumper, smoke bust) and the Mona Lisa
mirror-tile — is reverted to `letterbox`. The textural gains from those
treatments were real but small, and the consistency of "every screen
letterboxes the same way" is worth more than per-entry cleverness.
`mirror-tile` and `edge-clone` are kept as algorithm options in the
script for future entries (or HD-pack experimentation), but no current
entry uses them.

### Round 1 — `edge-clone` failures

(Recorded first; superseded by round 2's broader simplification, but the
underlying observation about per-row stretching is still the reason
`edge-clone` is no longer used.)

### Entry 4 — `PCR_MENU` (main-menu stormy sky/sea)

**Initial:** `edge-clone (dark)`. **Revised:** `palette-fill (dark sky)`.

`edge-clone` per-row stretching turned the cloud waves into visible
vertical stripes across both margins. The stripes are obvious against the
smooth sky gradient — the eye reads them as a rendering bug rather than a
natural extension.

`palette-fill` auto-detects a dark blue-grey from the leftmost+rightmost
column histogram and fills both margins with that single colour. The
result is nearly invisible — the eye reads "the picture has a dark
border", not "the picture has been extended." The `+HD` flag stays:
re-authoring would still beat both treatments here for the most prominent
screen the player sees.

### Entry 56 — spaceship approaching the volcano planet

**Initial:** `edge-clone (stars)`. **Revised:** `palette-fill (black)`.

The starfield-extension idea works for symmetric starfields (the bumper,
entry 2), but this composition has a glowing planet near the right edge
and an asymmetric nebula trail near the left. Per-row column stretch
turned the nebula into colourful horizontal bands and the planet's right
edge into vertical stripes of rocky terrain.

`palette-fill` auto-detects black (the dominant edge colour) and produces
the same result as `letterbox`. Visually identical, but logged as
`palette-fill` because it's coming from the bitmap's own palette rather
than a hardcoded RGB(0,0,0).

### Round-1 specifics (historical)

- **Entry 4** (`PCR_MENU`) `edge-clone → palette-fill`. Per-row stretching
  produced visible vertical stripes from the cloud waves. Round 2 later
  demoted further to `letterbox` (with `stretch`/`gradient` as candidates
  to revisit).
- **Entry 56** (spaceship + planet) `edge-clone → palette-fill`. Asymmetric
  nebula trail and planet glow near the edges produced banding. Round 2
  unified this with the rest at `letterbox`.

## Why `edge-clone` is no longer used

The per-row column-stretch algorithm amplifies any vertical structure
near the edges into visible stripes across the margin. Even on entries
where it "worked" (wood-grain maps, smoke, symmetric starfields), the
result was a band of horizontal repetition that read as a render bug to a
fresh viewer. Letterbox is honest: the eye reads black bars as
intentional framing, not as a corrupted extension. The cost-benefit
didn't justify per-entry treatment selection.

## PCR_MENU candidates

The one entry where letterbox felt wrong is the main-menu sky/sea
background, because the harsh black bars contrast badly with the
dark-grey-blue of the sea. Two alternative treatments are implemented in
the script for review:

- **`stretch`** — bilinear horizontal resample from 640 to 853. Keeps the
  LBA stormy-ocean mood but visibly distorts the waves and the bloom
  highlight by ~33%. Low engine cost (one `SDL_Surface` blit with
  scaling); zero asset work.
- **`gradient`** — ignore the bitmap, render a procedural blue gradient
  (deep midnight at top → slate at the horizon). Removes the LBA art
  entirely. Lowest engine cost (no HQR load). Most flexible (can retune
  colours per build, can ship "dark mode" variants).

Both are open candidates pending review. Render them with:

```bash
scripts/dev/art_treatment_preview.py /path/to/SCREEN.HQR --entry 4 \
    --compare --out /tmp/menu_compare.png
```

## What's next

1. Decide between `stretch` and `gradient` for `PCR_MENU`, then update
   the triage one last time.
2. Once the engine is widescreen-capable (Phase 3 of
   [WIDESCREEN.md](../WIDESCREEN.md)), wire `BlitFullscreenBitmap()` per
   [callsite-map.md](callsite-map.md) and integrate the triage as data.
3. The cheap-test PNGs become goldens for a future
   `ui screen-bitmap N` capture verb so engine integration can byte-diff
   against the offline reference.
