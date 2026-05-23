# RESS.HQR catalog

Companion to [art-catalog.md](art-catalog.md). Where SCREEN.HQR is "39 pairs
of full-screen bitmaps", RESS.HQR is the engine's mixed-asset bag — palettes,
textures, manifests, and a font. Most entries are **not** UI bitmaps that hit
fixed pixel coordinates, so the widescreen surface is much smaller than the
slot count suggests.

## File layout

50 slots, 40 present (10 reserved holes; see [SOURCES/COMMON.H:109-159](../../SOURCES/COMMON.H)
for the comment-marked gaps). Every entry has a named `RESS_*` define in
`COMMON.H`. Entries are addressed by index from C code only; no script
bytecode reaches in here.

## Inventory

`#` = entry index. **Category** column: `palette` (LUT-only), `tex-3d`
(game-space 3D texture, no fixed-pixel concern), `tex-bg` (sky/sea wraparound
tile), `font` (glyph atlas, drawn at game-chosen coords), `manifest`
(bbox/index lookup table, not pixel content), `mesh-3d` (3D model),
`data` (other engine table).

| #  | Define | Category | What it is | Consumer |
|---|---|---|---|---|
| 0  | `RESS_PAL`            | palette   | Normal game palette                          | [AMBIANCE.CPP:425](../../SOURCES/AMBIANCE.CPP) → `PtrPalNormal` |
| 1  | `RESS_FONT_GPM`       | font      | Font glyph mask atlas (variable-width)       | [PERSO.CPP:2474](../../SOURCES/PERSO.CPP) → `LbaFont` + `SetFont(LbaFont, 2, 8)` |
| 2  | `RESS_EXT_SIZE_INFO`  | data      | 16-byte sizing table for HQ buffer allocations | [MEM.CPP:88](../../SOURCES/MEM.CPP) |
| 3  | —                     | —         | (hole)                                       | — |
| 4  | —                     | —         | (hole — was `RESS_INIT_PLASMA`)              | — |
| 5  | `RESS_GOODIES_GPC`    | manifest  | Bbox manifest for sprites in `sprites.hqr`   | [PERSO.CPP:2537](../../SOURCES/PERSO.CPP) → `PtrZvExtra[N*8 + {x,y,…}]` |
| 6  | `RESS_TEXTURES`       | tex-3d    | 256×256 indexed texture page for 3D objects  | [PERSO.CPP:2529](../../SOURCES/PERSO.CPP) → `BufferTexture` |
| 7  | `RESS_GAME_OVER`      | mesh-3d   | 3D model rendered with `SetProjection(320,240,…)` zoom | [GAMEMENU.CPP:3924](../../SOURCES/GAMEMENU.CPP) |
| 8  | `RESS_GOODRAW_GPC`    | manifest  | Bbox manifest for sprites in `spriraw.hqr`   | [PERSO.CPP:2542](../../SOURCES/PERSO.CPP) → `PtrZvExtraRaw` |
| 9  | `RESS_PAL_BLACK`      | palette   | All-black palette (fade target)              | [AMBIANCE.CPP:433](../../SOURCES/AMBIANCE.CPP) → `PtrPalBlack` |
| 10 | `RESS_PAL_ECLAIR`     | palette   | Lightning-flash palette                      | [AMBIANCE.CPP:429](../../SOURCES/AMBIANCE.CPP) → `PtrPalEclair` |
| 11 | `RESS_SKYSEA0`        | tex-bg    | Citadel sky/sea 256×256 wraparound tile       | [EXTFUNC.CPP:152-174](../../SOURCES/EXTFUNC.CPP) → `SkySeaTexture` |
| 12 | `RESS_SKYSEA1`        | tex-bg    | Puits de Sendell sky/sea                     | same path, `Island`-indexed |
| 13 | `RESS_SKYSEA2`        | tex-bg    | Desert sky/sea                                | " |
| 14 | `RESS_SKYSEA3`        | tex-bg    | Emerald Moon sky/sea                          | " |
| 15 | `RESS_SKYSEA4`        | tex-bg    | Otringal sky/sea                              | " |
| 16 | `RESS_SKYSEA5`        | tex-bg    | Celebrat sky/sea                              | " |
| 17 | `RESS_SKYSEA6`        | tex-bg    | Blafards/Plateforme sky/sea                   | " |
| 18 | `RESS_SKYSEA7`        | tex-bg    | Mosquibees sky/sea                            | " |
| 19 | `RESS_SKYSEA8`        | tex-bg    | Knartas sky/sea                               | " |
| 20 | `RESS_SKYSEA9`        | tex-bg    | Îlot CX sky/sea                               | " |
| 21 | `RESS_SKYSEA10`       | tex-bg    | Ascenseur sky/sea                             | " |
| 22-25 | —                 | —         | (holes)                                      | — |
| 26 | `RESS_SKYSEA00`       | tex-bg    | Citabau sky/sea (special citadel variant)    | [EXTFUNC.CPP:160](../../SOURCES/EXTFUNC.CPP) |
| 27 | `RESS_XPL0`           | palette   | Citadel XPL pack — palette + fog + transparency | [AMBIANCE.CPP:446-451](../../SOURCES/AMBIANCE.CPP) → `PtrXplPalette` |
| 28-37 | `RESS_XPL1..10`   | palette   | Per-island XPL packs (same layout as XPL0)   | same path, `Island`-indexed |
| 38-41 | —                 | —         | (holes)                                      | — |
| 42 | `RESS_XPL00`          | palette   | Citabau XPL pack                              | [AMBIANCE.CPP:444](../../SOURCES/AMBIANCE.CPP) |
| 43 | `RESS_ANIM3DS_GPC`    | manifest  | Bbox manifest for 3DS animation sprites      | [PERSO.CPP:2548](../../SOURCES/PERSO.CPP) → `PtrZvAnim3DS` |
| 44 | `RESS_FILE3D`         | data      | 3D file directory                             | [PERSO.CPP:2519](../../SOURCES/PERSO.CPP) → `BufferFile3D` |
| 45 | `RESS_FLOW`           | data      | Flow-particle effect descriptions            | [FLOW.CPP:112](../../SOURCES/FLOW.CPP) → `TabPartFlow[]` |
| 46 | `RESS_POF`            | data      | POF data table (offset-indexed)              | [PERSO.CPP:2588](../../SOURCES/PERSO.CPP), [POF.CPP:14](../../SOURCES/POF.CPP) → `BufferPof` |
| 47 | `RESS_IMPACT`         | data      | Impact (collision FX) effect table           | [PERSO.CPP:2592](../../SOURCES/PERSO.CPP), [IMPACT.CPP:434](../../SOURCES/IMPACT.CPP) → `BufferImpact` |
| 48 | `RESS_ACFLIST`        | data      | ACF (Adeline Cinematic Format) playlist      | [PLAYACF.CPP:63](../../SOURCES/PLAYACF.CPP) → `ListAcf` |
| 49 | —                     | —         | (hole)                                       | — |

## Widescreen relevance, by category

**`font` (1 entry)** — `RESS_FONT_GPM`. Glyphs are drawn at game-chosen XY
via `Font(x, y, str)` → [LIB386/SYSTEM/SYSFONT.CPP](../../LIB386/SYSTEM/SYSFONT.CPP).
The font *bitmap* is fine for widescreen; what needs auditing is the
hardcoded XY positions of text calls (separate effort, not asset-side).

**`tex-bg` (12 entries — `RESS_SKYSEA*`)** — Each is a 256×256 wraparound
tile rendered by `DrawSkySeaZBuf()` and the sky-fill pipeline. The tile is
already in the right shape for widescreen; what needs to change is the engine
math that wraps it across the framebuffer. Render-pipeline scope, not asset
scope.

**`tex-3d` (1 entry — `RESS_TEXTURES`)** — 256×256 texture page consumed by
the 3D pipeline. Game-space — projection scales it for free.

**`mesh-3d` (1 entry — `RESS_GAME_OVER`)** — 3D model. Caller hardcodes
`SetProjection(320, 240, …)` at [GAMEMENU.CPP:3932](../../SOURCES/GAMEMENU.CPP).
The model is fine; the projection centre needs fixing — already tracked in
the `[[project_projection_4_3_audit]]` line of the widescreen plan.

**`palette` (15 entries)** — `RESS_PAL`, `RESS_PAL_BLACK`, `RESS_PAL_ECLAIR`,
and the twelve per-island `RESS_XPL*` packs. LUT-only; no pixel data; no
widescreen concern.

**`manifest` (3 entries)** — `RESS_GOODIES_GPC`, `RESS_GOODRAW_GPC`,
`RESS_ANIM3DS_GPC`. Bbox/index lookup tables that point into the sprite
HQRs. No pixels here, but the offsets they encode are content-dependent —
see [sprite-hud-sites.md](sprite-hud-sites.md) for where those sprites
actually land on screen.

**`data` (5 entries)** — `RESS_EXT_SIZE_INFO`, `RESS_FILE3D`, `RESS_FLOW`,
`RESS_POF`, `RESS_IMPACT`, `RESS_ACFLIST`. Engine tables. No pixels.

## Summary

Of 40 present entries, the asset-side widescreen surface is:
1. **`RESS_FONT_GPM`** — font as-is, callers need XY audit
2. **`RESS_GAME_OVER`** — model as-is, projection centre needs fix
3. **`RESS_SKYSEA*`** — tiles as-is, render pipeline needs wider wrap

Three entries, two of which are render-pipeline work that doesn't touch the
asset. **There is no `RESS_HQR_NAME` work to do for widescreen on the asset
side.** This was the deliverable: confirmation that the engine-resources
archive is not a blocker for widescreen art treatment.
