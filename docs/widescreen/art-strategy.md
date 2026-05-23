# Widescreen art treatment strategy

Per-entry plan for SCREEN.HQR: how each 640×480 bitmap should land in a wider
framebuffer (target 16:9 → 853×480 at default height, per
[WIDESCREEN.md](../WIDESCREEN.md#target-behaviour)). Companion to
[art-catalog.md](art-catalog.md) (what each entry depicts) and
[callsite-map.md](callsite-map.md) (where each is loaded). Reading those
first is recommended; this doc assumes that context.

The other companion catalogues — [hqr-overview.md](hqr-overview.md),
[ress-catalog.md](ress-catalog.md), [sprite-hud-sites.md](sprite-hud-sites.md)
— confirmed that SCREEN.HQR is the entire asset-side widescreen surface.
This doc is therefore the **complete** art-strategy deliverable.

## Treatment categories

Five buckets. The first four are runtime treatments (engine renders the
existing 640×480 bitmap into the wider framebuffer); the fifth is an
optional future re-author.

### `letterbox`

Centre-blit the 640×480 bitmap and fill the side margins with a flat
palette index (typically black, index 0). Always correct, always safe,
visually dated.

**Use when:** the bitmap has a frame baked in (porthole vignette, wooden
frame), or an asymmetric/composed scene where extending edges would feel
wrong, or when no margin treatment beats letterbox in cost/quality.

### `palette-fill`

Centre-blit and fill the side margins with a single palette index sampled
from the bitmap's outermost column (or hand-picked from the palette). One
flat colour, no seams. Cheap.

**Use when:** the bitmap's left and right edges are a uniform colour
(white logo background, pure black starfield/space, dark wood). Indistinguishable
from a bitmap that "always extended that colour."

### `edge-clone`

Centre-blit and fill the margins by replicating the leftmost and rightmost
column (or a small averaged edge band) outward. Works for content with a
continuous, low-frequency edge.

**Use when:** the edge is a natural extension surface — wood grain, sky
gradient, sea horizon, smoke. Visible seam only if the edge has
high-frequency detail that the eye can latch onto.

### `mirror-tile`

Mirror the edge band (e.g. leftmost 100 columns) into the margin. The
join is at the seam itself; the mirrored content extends outward.

**Use when:** the bitmap has a repeating pattern that mirrors symmetrically
(wallpaper, dense star field, repeating texture). Rare in this set —
mostly the Mona Lisa portrait's wallpaper qualifies.

### `stretch`

Resample the bitmap horizontally from 640 to the target width (bilinear).
No margins — the whole frame is covered, but content is distorted ~33%
wider at 16:9. Acceptable for natural imagery (sky, sea); poor for text
or structured composition.

**Use when:** letterbox feels wrong (the bitmap's content is mostly
atmosphere not detail) and the distortion is tolerable.

### `gradient`

Replacement treatment: ignore the bitmap, render a procedural vertical
gradient instead. Lowest engine cost (no HQR load), most flexible (can
retune colours), zero fidelity to the original bitmap.

**Use when:** no runtime treatment of the original art reads well and the
backdrop's job is purely tonal (e.g. behind a menu).

### `hd-pack-later`

Future re-authored bitmap shipped in a separate optional HQR. Engine prefers
the HD pack when present, falls back to the runtime treatment otherwise.
Not delivered by this strategy — recorded here for entries that would
visibly benefit from a higher-resolution re-author.

**Use when:** the runtime treatment is "letterbox" *and* the bitmap is
prominent enough that black bars feel dated. Tagged alongside the runtime
treatment, not instead of it.

## Triage

After two rounds of cheap-test review (see
[cheap-test-findings.md](cheap-test-findings.md)) the triage simplified
to **letterbox by default**, with two narrow exceptions:

- The three pure-white-background distributor/dev logos (`PCR_LOGO`,
  `PCR_ACTIVISION`, `PCR_VIRGIN`), where `palette-fill` auto-detects
  white and the fill is genuinely invisible.
- `PCR_MENU`, where letterbox's harsh black bars contrast badly with the
  dark stormy sea — an open question between `stretch` and `gradient`,
  decided against rendered output.

Earlier rounds tried per-entry `edge-clone`, `mirror-tile`, and various
`palette-fill` (non-white) calls. None of those gains held up under a
broader visual look — the textural illusions were small, the failure
modes (stripes, duplicate-edge artifacts, visible non-black margin
fills) were obvious. `letterbox` is honest: the eye reads black bars as
intentional framing, not as a corrupted extension.

`+HD?` flags entries that would benefit from a future HD-pack re-author.

| #  | Entry | Treatment | +HD? | Rationale |
|---|---|---|---|---|
| 0  | `PCR_LOGO`       | `palette-fill` (white) |   | Pure white background; auto-detected fill is invisible. |
| 2  | `PCR_BUMPER`     | `letterbox`            | ✓ | One-shot intro card; HD bumps would be felt. |
| 4  | `PCR_MENU`       | **open** — `letterbox` / `stretch` / `gradient` | ✓ | Stormy sea/sky. Letterbox is the fallback; `stretch` (resample 640→853, ~33% distortion, keeps LBA mood) and `gradient` (procedural blue replacement, no bitmap) are the candidates. Decide against rendered output. |
| 6  | `PCR_ARDOISE`    | `letterbox`            |   | Slate frame is the content. |
| 8  | `PCR_PEGOUT`     | `letterbox`            |   | Paper-on-wood sewer map. Orphan in current code. |
| 10 | `PCR_AEGOUT`     | `letterbox`            |   | Slate-grid sewer map. |
| 12 | `PCR_PLABYRT`    | `letterbox`            |   | Paper labyrinth map. Orphan. |
| 14 | `PCR_ALABYRT`    | `letterbox`            |   | Slate-grid labyrinth map. |
| 16 | `PCR_PLUNE`      | `letterbox`            |   | Paper moon map. Orphan. |
| 18 | `PCR_ALUNE`      | `letterbox`            |   | Slate-grid moon map. |
| 20 | `PCR_HACIENDA`   | `letterbox`            |   | Cliff cove with baked circular vignette. Orphan. |
| 22 | `PCR_VUPHARE`    | `letterbox`            | ✓ | Lighthouse establishing shot, baked frame. Orphan. |
| 24 | `PCR_VUPHARE2`   | `letterbox`            | ✓ | Lighthouse, porthole vignette. |
| 26 | —                | `letterbox`            |   | Planet + comet on space. |
| 28 | —                | `letterbox`            |   | Funfrock lab interior. |
| 30 | —                | `letterbox`            |   | Picnic scene. |
| 32 | —                | `letterbox`            | ✓ | Emerald Moon close-up through porthole. |
| 34 | —                | `letterbox`            |   | Cell with Sendell orbs. |
| 36 | —                | `letterbox`            |   | Celestial diagram. |
| 38 | —                | `letterbox`            |   | Mona Lisa portrait (mirror-tile was a viable alternative but letterbox is consistent). |
| 40 | —                | `letterbox`            |   | Volcano paper map. |
| 42 | —                | `letterbox`            |   | Volcano slate map. |
| 44 | —                | `letterbox`            |   | Album + guard close-up. |
| 46 | —                | `letterbox`            |   | Bust on plinth, smoky backdrop. |
| 48 | —                | `letterbox`            |   | Soldier musicians on parade. |
| 50 | —                | `letterbox`            |   | Prison cell. |
| 52 | —                | `letterbox`            |   | Twinsen vs dragon. |
| 54 | —                | `letterbox`            |   | Red-sky forum. |
| 56 | —                | `letterbox`            |   | Spaceship + planet on starfield. |
| 58 | —                | `letterbox`            |   | Coin/marble celebration. |
| 60 | `PCR_CDROM`      | `letterbox`            |   | CD insert prompt. Player sees this often. |
| 62 | —                | `letterbox`            |   | Sendell-baby orb. |
| 64 | —                | `letterbox`            |   | Twinsen's-house paper map. |
| 66 | —                | `letterbox`            |   | Twinsen's-house slate-grid map. |
| 68 | —                | `letterbox`            |   | Twinsen close-up with medallion. |
| 70 | —                | `letterbox`            |   | Sendell-form Twinsen. |
| 72 | `PCR_ACTIVISION` | `palette-fill` (white) |   | Pure white background; auto-detected fill is invisible. |
| 74 | `PCR_EA`         | `letterbox`            |   | Black background; letterbox is identical to palette-fill (black). |
| 76 | `PCR_VIRGIN`     | `palette-fill` (white) |   | Pure white background; auto-detected fill is invisible. |

### Distribution

| Treatment | Count | Where it applies |
|---|---|---|
| `letterbox`    | 35 | Almost everything. The honest default; what the eye reads as intentional framing. |
| `palette-fill` |  3 | Pure-white-background logos (`PCR_LOGO`, `PCR_ACTIVISION`, `PCR_VIRGIN`) where the fill is invisible. |
| **open**       |  1 | `PCR_MENU` — choice between `letterbox` / `stretch` / `gradient` pending visual review. |
| `edge-clone`, `mirror-tile` | 0 | Available as algorithms in the script, no current entry uses them. Kept for future entries or HD-pack experiments. |
| `hd-pack-later` (additional) | 5 | `PCR_MENU`, `PCR_BUMPER`, `PCR_VUPHARE`, `PCR_VUPHARE2`, Emerald-Moon porthole. The prominent letterboxed entries where re-authored art would be felt. |

## Cheap tests (pre-engine validation)

We don't need a wider framebuffer in the engine to validate the
treatments. The cheapest, lowest-risk path is a host-side offline preview:
take a bitmap + palette, apply each treatment at a target width, dump PNG.
Visual review answers "does this look right?" before any engine plumbing.

### Test 1 — treatment preview script (done)

[`scripts/dev/art_treatment_preview.py`](../../scripts/dev/art_treatment_preview.py)
implements the four treatments and dumps target-width PNGs:

```bash
scripts/dev/art_treatment_preview.py /path/to/SCREEN.HQR --all --outdir preview/
scripts/dev/art_treatment_preview.py /path/to/SCREEN.HQR --entry 4 --compare --out preview/menu_compare.png
```

Findings from the first pass are recorded in
[cheap-test-findings.md](cheap-test-findings.md). Two strategy calls were
revised after seeing 853-wide output (entries 4 and 56 — both moved from
`edge-clone` to `palette-fill`); 37 calls held up. The script's embedded
triage table reflects the revisions and is the source of truth for `--all`
runs.

The PNG dumps land under `docs/widescreen/catalog-dumps/` alongside the
catalog extractor's output — same `.gitignore` covers them (copyright).

### Test 2 — palette boundary check

Sample the leftmost and rightmost column of every bitmap and check the
palette index distribution. Entries with a single dominant index at both
edges are `palette-fill` candidates; entries with high column variance
need `edge-clone` or `letterbox`. This is a confidence pass on the triage
above — runnable in ten lines of Python against the same extractor.

### Test 3 — engine-side integration (post-Phase 3)

Once the engine has a wider framebuffer (Phase 3 of
[WIDESCREEN.md](../WIDESCREEN.md)), wire a single helper:

```cpp
// new in LIB386/SYSTEM/HQRLOAD.CPP or a sibling
void BlitFullscreenBitmap(U8 *dest, const U8 *src, U8 *palette,
                          BitmapTreatment treatment);
```

Patch one call site first — [GAMEMENU.CPP:2375 `MainGameMenu()`](../../SOURCES/GAMEMENU.CPP)
where `PCR_MENU` loads. Boot the game, screenshot, compare to the cheap-test
PNG. If they match, roll the helper out to the other eleven sites
enumerated in [callsite-map.md](callsite-map.md).

The cheap test PNGs become the goldens for an automated UI capture verb
(`ui screen-bitmap N`) following the pattern from
[docs/CONTROL.md](../CONTROL.md#ui-capture) once that lands.

### Test 4 — HD-pack hook (optional, late)

Adding an HD-pack lookup is a one-line addition to `BlitFullscreenBitmap`:

```cpp
if (HQR_TryLoad(HD_SCREEN_HQR, dest, entry_index)) return;  // HD pack hit
// fall through to runtime treatment
```

Defer until there's an actual HD pack to load — until then, every entry
runs its runtime treatment. The five `+HD?` flagged entries are the
candidates a future contributor (AI-upscale, hand-redrawn, whatever) would
target first.

## Expected outcome

After Phase 5 of [WIDESCREEN.md](../WIDESCREEN.md) consumes this strategy:

1. **No re-authored artwork in the repo.** Retail bitmaps stay verbatim;
   the engine treats them at runtime. Copyright stays clean.
2. **One helper function** (`BlitFullscreenBitmap`) covers all twelve
   SCREEN.HQR call sites listed in [callsite-map.md](callsite-map.md).
3. **Per-entry treatment is data, not code.** A small table (entry index →
   treatment + parameters) drives the helper. New entries (or revised
   triage) are a data edit, not a code change.
4. **HD pack is opt-in and additive.** Players without it see the runtime
   treatment; players with it see re-authored art. Either is correct.

This is a no-engine-changes-yet PR. The treatment table here is the
specification the engine-side work will implement, and the cheap-test
script is the first concrete artifact.
