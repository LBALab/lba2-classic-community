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

### `hd-pack-later`

Future re-authored bitmap shipped in a separate optional HQR. Engine prefers
the HD pack when present, falls back to the runtime treatment otherwise.
Not delivered by this strategy — recorded here for entries that would
visibly benefit from a higher-resolution re-author.

**Use when:** the runtime treatment is "letterbox" *and* the bitmap is
prominent enough that black bars feel dated. Tagged alongside the runtime
treatment, not instead of it.

## Triage

The runtime treatment column is what the engine should apply by default
once Phase 5 wires up. `+HD?` flags entries that are reasonable HD-pack
candidates for a future re-author initiative.

| #  | Entry | Treatment | +HD? | Rationale |
|---|---|---|---|---|
| 0  | `PCR_LOGO`       | `palette-fill` (white) |   | Pure white background; flat fill is invisible. |
| 2  | `PCR_BUMPER`     | `edge-clone`           | ✓ | Starfield extends naturally; bumper is a one-shot intro card so HD bumps would be felt. |
| 4  | `PCR_MENU`       | `palette-fill` (dark sky) | ✓ | Stormy sea/sky. **Revised from `edge-clone` after cheap-test review** (see [cheap-test-findings.md](cheap-test-findings.md)): per-row stretching produced visible vertical stripes from the cloud waves. Auto-detected dark blue-grey blends invisibly. Prominent (main menu) → HD candidate. |
| 6  | `PCR_ARDOISE`    | `palette-fill` (dark wood) |   | The slate frame is the content; surrounding wood is dark and uniform. |
| 8  | `PCR_PEGOUT`     | `edge-clone` (wood)    |   | Paper-on-wood map; wood grain extends. Orphan in current code — low priority. |
| 10 | `PCR_AEGOUT`     | `palette-fill` (black) |   | Slate-grid sewer map on black; flat fill matches. |
| 12 | `PCR_PLABYRT`    | `edge-clone` (wood)    |   | Same as pegout. Orphan. |
| 14 | `PCR_ALABYRT`    | `palette-fill` (black) |   | Same as aegout. |
| 16 | `PCR_PLUNE`      | `edge-clone` (wood)    |   | Same as pegout. Orphan. |
| 18 | `PCR_ALUNE`      | `palette-fill` (black) |   | Same as aegout. |
| 20 | `PCR_HACIENDA`   | `letterbox`            |   | Cliff cove inside a baked circular vignette. Orphan in current code. |
| 22 | `PCR_VUPHARE`    | `letterbox`            | ✓ | Lighthouse + baked framed border; extending breaks the frame. Orphan, but recognisable LBA establishing shot. |
| 24 | `PCR_VUPHARE2`   | `letterbox`            | ✓ | Porthole vignette. Same shape as 22. |
| 26 | —                | `palette-fill` (black) |   | Planet+comet on black space; flat fill matches. |
| 28 | —                | `letterbox`            |   | Funfrock lab interior, architectural composition. |
| 30 | —                | `letterbox`            |   | Picnic scene, composed and dark-bordered. |
| 32 | —                | `letterbox`            | ✓ | Emerald Moon close-up through porthole; vignette baked. |
| 34 | —                | `letterbox`            |   | Cell with orbs, composed dark interior. |
| 36 | —                | `palette-fill` (black) |   | Celestial diagram centred on black. |
| 38 | —                | `mirror-tile`          |   | Blue floral wallpaper repeats; mirror-tile preserves the pattern outward from the candles. The one strong mirror-tile candidate in the set. |
| 40 | —                | `edge-clone` (paper)   |   | Volcano paper map; wood/paper grain extends. |
| 42 | —                | `palette-fill` (black) |   | Volcano slate map on black. |
| 44 | —                | `letterbox`            |   | Asymmetric close-up of album + bald guard. |
| 46 | —                | `edge-clone` (smoke)   |   | Bust on plinth with smoky background; smoke clones reasonably. `letterbox` is fine fallback. |
| 48 | —                | `letterbox`            |   | Soldier musicians on parade, composed scene. |
| 50 | —                | `letterbox`            |   | Prison cell, top-down composition. |
| 52 | —                | `letterbox`            |   | Twinsen vs dragon, fire breath. |
| 54 | —                | `letterbox`            |   | Forum/temple with red sky and dragon silhouette. |
| 56 | —                | `palette-fill` (black) |   | Spaceship + planet on starfield. **Revised from `edge-clone` after cheap-test review** (see [cheap-test-findings.md](cheap-test-findings.md)): asymmetric nebula trail and planet glow near the edges produced banding. Auto-detected black is visually equivalent to `letterbox` here. |
| 58 | —                | `palette-fill` (black) |   | Coin/marble celebration centred on black. |
| 60 | `PCR_CDROM`      | `palette-fill` (black) |   | Dancing couple on disc, pure black background. Player sees this often (CD check) — keep it crisp. |
| 62 | —                | `palette-fill` (black) |   | Sendell-baby orb on black. |
| 64 | —                | `edge-clone` (paper)   |   | Twinsen's-house paper map on cardboard. |
| 66 | —                | `palette-fill` (black) |   | Twinsen's-house slate-grid version on black. |
| 68 | —                | `letterbox`            |   | Twinsen close-up with medallion, cloud-and-bloom backdrop — clone may seam, letterbox is safe. |
| 70 | —                | `letterbox`            |   | Sendell-form Twinsen, sky+ocean backdrop — same concern as 68. |
| 72 | `PCR_ACTIVISION` | `palette-fill` (white) |   | Pure white background. |
| 74 | `PCR_EA`         | `palette-fill` (black) |   | Pure black background. |
| 76 | `PCR_VIRGIN`     | `palette-fill` (white) |   | Pure white background. |

### Distribution

| Treatment | Count | Where it applies |
|---|---|---|
| `palette-fill` | 18 | All slate maps (black), splash logos (white/black), centred-on-black cinematic stills, plus the cheap-test-revised entries (4, 56). The dominant treatment because LBA leans on flat backgrounds. |
| `letterbox`    | 15 | Vignetted shots, asymmetric composed scenes. The safe default. |
| `edge-clone`   |  5 | Wood-grain map backgrounds (paper P maps), symmetric starfield (bumper), smoke. Where the edge is a low-frequency natural extension. |
| `mirror-tile`  |  1 | Mona Lisa wallpaper — the only entry with a clear tiling pattern. |
| `hd-pack-later` (additional) | 5 | The most prominent letterboxed entries — main-menu sky, bumper, lighthouse shots, Emerald-Moon porthole. Tagged for a future re-author, not blocking. |

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
