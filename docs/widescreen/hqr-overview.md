# Widescreen HQR overview

Retail data is sixteen `.HQR` archives. This doc triages each one by
widescreen relevance, so the asset-side work is bounded before we start on
treatment. Read [art-catalog.md](art-catalog.md) for `SCREEN.HQR` (done) and
[ress-catalog.md](ress-catalog.md) / [sprite-hud-sites.md](sprite-hud-sites.md)
for the next two in-scope archives.

Companion to the SCREEN.HQR catalog; same evidence-first style.

## Archive table

Names from [SOURCES/COMMON.H:66-81](../../SOURCES/COMMON.H). "Scope" is "does
the rendered output land at fixed pixel coordinates on a 4:3-assumed framebuffer."

| HQR | Slots | Content | Scope | Notes |
|---|---|---|---|---|
| `screen.hqr`  | 78 (39 pairs)   | Full-screen indexed bitmaps + palettes        | **Yes**       | See [art-catalog.md](art-catalog.md). Catalogued. |
| `ress.hqr`    | 50 (40 present) | Mixed engine assets — fonts, palettes, sky/sea + 3D textures, manifests | **Partial** | See [ress-catalog.md](ress-catalog.md). Only the font hits fixed UI coords; other entries are textures (game-space) or data tables. |
| `sprites.hqr` | 426             | Variable-size sprites, on-demand-loaded       | **Partial**   | Most are game-world (auto-scaled by projection). HUD-fixed call sites enumerated in [sprite-hud-sites.md](sprite-hud-sites.md). |
| `spriraw.hqr` | 168             | Raw (uncompressed) sprite variants            | **Partial**   | Same shape as sprites.hqr; same triage. |
| `scrshot.hqr` | (varies)        | 640×480 + palette pairs — attract-mode slideshow | **Yes**     | Same shape and treatment as `screen.hqr`. Loaded by [GAMEMENU.CPP:4163 `SlideShow()`](../../SOURCES/GAMEMENU.CPP). |
| `holomap.hqr` | 47 (41 present) | Holomap globe textures + 3D arrow/vehicle models + per-planet height/grid maps | **Render-path** | Globe is 3D; only overlaid UI text/arrows are 2D. See HOLO.H for indices. Out of asset scope, in render-pipeline scope. |
| `lba_bkg.hqr` | 18102           | Isometric tile backgrounds for indoor scenes  | **Render-path** | Drawn into viewport, not fixed coords. Phase 5 of widescreen plan covers this — see [docs/WIDESCREEN.md](../WIDESCREEN.md). |
| `lba2.hqr`    | (varies)        | Credits text/data                             | No (text-only) | `CREDITS_HQR_NAME` despite the generic name. |
| `video.hqr`   | (varies)        | Smacker video bitstreams (cutscenes)          | **Render-path** | Videos render into a viewport; scaling concern not asset concern. |
| `samples.hqr` | (varies)        | PCM audio samples                             | No | |
| `anim.hqr`    | (varies)        | Animation keyframe data                       | No (game-space) | |
| `anim3ds.hqr` | (varies)        | 3DS-style animation tracks                    | No (game-space) | |
| `body.hqr`    | (varies)        | 3D character/object meshes                    | No (game-space) | |
| `objfix.hqr`  | (varies)        | Static prop meshes                            | No (game-space) | |
| `scene.hqr`   | (varies)        | Scene layouts, entity placements, life/track scripts | No | |
| `text.hqr`    | (varies)        | Localised dialog strings                      | No | |

## Scope summary

**Asset catalog needed:** `screen.hqr` (done), `scrshot.hqr` (same shape as
screen.hqr — same treatment), `ress.hqr` (mostly engine data, font is the
relevant asset), `sprites.hqr` + `spriraw.hqr` (only a handful of HUD-fixed
sites).

**Render-path needed but not asset work:** `holomap.hqr`, `lba_bkg.hqr`,
`video.hqr`. The bitmap content is in the right shape (tiles, viewport-bound
videos, 3D models); what needs work is the engine code that draws them.
Covered by the engine-side phases of [WIDESCREEN.md](../WIDESCREEN.md).

**Out of scope entirely:** `lba2.hqr`, `samples.hqr`, `anim.hqr`,
`anim3ds.hqr`, `body.hqr`, `objfix.hqr`, `scene.hqr`, `text.hqr`. No pixels,
or pixels are game-space.

That leaves the **two** companion catalogs called for here: RESS (data-heavy,
small actual UI surface) and SPRITE HUD sites (precise enumeration of where
sprites land at fixed coords). `scrshot.hqr` doesn't get its own catalog
because the SCREEN.HQR analysis applies verbatim.
