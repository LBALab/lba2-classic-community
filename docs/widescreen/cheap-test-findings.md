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

Two entries demoted from `edge-clone` to `palette-fill` after visual
review. The script's embedded `TRIAGE` table reflects the revisions;
[art-strategy.md](art-strategy.md) is the historical record of the initial
call and should be updated alongside.

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

## Confirmed picks

The remaining strategy calls held up under visual inspection.

- **`PCR_LOGO` (0) `palette-fill (white)`** — invisible fill, logo
  perfectly centred.
- **`PCR_BUMPER` (2) `edge-clone (stars)`** — starfield extends cleanly.
  The bumper has no high-contrast asymmetric content near the edges,
  unlike entry 56.
- **`PCR_PEGOUT` (8) `edge-clone (wood)`** — wood-grain background
  extends with mild banding that reads as weathered texture, not as a
  rendering artifact. Same pattern for the other paper-map P entries
  (12, 16, 40, 64).
- **`PCR_ARDOISE` (6) `palette-fill (dark wood)`** — auto-detection lands
  on a near-black that matches the surround. The slate frame floats
  cleanly on the dark.
- **Slate-grid A maps (10, 14, 18, 42, 66) `palette-fill (black)`** —
  auto-detected black is exact.
- **Entry 38 (Mona Lisa) `mirror-tile (wallpaper)`** — the wallpaper
  pattern extends correctly. Side effect: the candles get duplicated. The
  duplicate-candle artifact is recognisable as a treatment limitation
  rather than corrupted art; preferable to `edge-clone`'s stripes or
  `palette-fill`'s flat dark wallpaper colour. (See the `--compare` grid
  to confirm — it's the cleanest comparison in the set.)
- **Entry 46 (bust on plinth) `edge-clone (smoke)`** — smoke extends with
  mild banding that reads as natural cloud variation.
- **All `letterbox` entries** — vignettes and composed scenes look right
  with black bars; no edge content to extend.

## Generalisation

The pattern across entries:

- **`edge-clone` works when** the edges are low-frequency continuous
  texture (starfield, wood, smoke), regardless of what's in the centre.
- **`edge-clone` fails when** there's high-contrast content (clouds,
  bloom, glowing objects) anywhere near the edges. Per-row stretching
  amplifies any vertical structure into stripes.
- **`palette-fill` is the next-cheapest fallback** when `edge-clone`
  fails. It loses the textural illusion but is always seam-free.
- **`mirror-tile` works when** the bitmap has a continuous pattern at the
  edges (wallpaper, tiled floor). Rare in this set; one entry only.
- **`letterbox` is correct whenever** the bitmap has a baked frame
  (vignette, decorative border) — extending edges would break the frame.

In practice: when in doubt between `edge-clone` and `palette-fill`,
prefer `palette-fill`. The textural gain from `edge-clone` is small; the
risk of visible stripes is real.

## What's next

1. Update [art-strategy.md](art-strategy.md) to reflect the two revisions
   (and link this doc as the validation record).
2. Once the engine is widescreen-capable (Phase 3 of
   [WIDESCREEN.md](../WIDESCREEN.md)), wire `BlitFullscreenBitmap()` per
   [callsite-map.md](callsite-map.md) and integrate the triage as data.
3. The cheap-test PNGs become goldens for a future
   `ui screen-bitmap N` capture verb so engine integration can byte-diff
   against the offline reference.
