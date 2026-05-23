# Sprite HUD callsite map

Companion to [art-catalog.md](art-catalog.md) and [ress-catalog.md](ress-catalog.md).
SPRITES.HQR (426 entries) and SPRIRAW.HQR (168 entries) hold the engine's
small bitmaps — items, projectiles, item frames, UI corners. Most are drawn
in game space (XY derived from 3D projection of a world-space anchor) and
auto-scale with the viewport; a smaller number land at **hardcoded HUD
coordinates** and need widescreen attention.

This doc enumerates the hardcoded HUD sites. The game-space majority is
covered implicitly by Phase 2 (projection correction) of the
[widescreen plan](../WIDESCREEN.md).

## Loader / blit primitives

```cpp
// SOURCES/INTEXT.CPP:285
void PtrAffGraph(S32 xp, S32 yp, S32 sprite);   // looks up sprite in HQRPtrSprite by index
// LIB386/H/SVGA/MASK.H
void AffGraph(S32 num, S32 x, S32 y, const void *gph);  // raw blit, num is frame in atlas
```

Both ultimately call `AffGraph` which blits a mask-format sprite to `Log` at
the given screen XY, honouring `ScaleFactorSprite` (`DEF_SCALE_FACTOR = 256`
for HUD; scaled for game-world).

Sprite indices are named in [SOURCES/COMMON.H:409-459](../../SOURCES/COMMON.H):
`SYS_SPRITE_*` for system/UI sprites (frames, radio portraits) and `SPRITE_*`
for game-world ones (life heart, magic ball, coins, projectile balls, etc.).

## Hardcoded HUD sites

Coordinates inferred from the call sites and their surrounding code. "Anchor"
is the screen-space anchor the offsets are computed against — `(0, 0)`
top-left or a CTRL/PLAN region inside.

### Behaviour-menu HUD ([COMPORTE.CPP:251 `DrawInfoMenu`](../../SOURCES/COMPORTE.CPP))

The "info bar" shown alongside the behaviour wheel (Twinsen's pause menu).
Caller at [COMPORTE.CPP:368](../../SOURCES/COMPORTE.CPP) passes
`(CTRL_X0, CTRL_Y1 + 10)` = `(100, 300)`. The bar itself is **450 × 80**,
hardcoded at lines 89-92:

```cpp
#define CTRL_X0 100
#define CTRL_Y0 100
#define CTRL_X1 550
#define CTRL_Y1 290
```

Sprites inside the bar (relative to `(x0, y0) = (100, 300)`):

| Sprite | Offset | Define |
|---|---|---|
| Life heart icon         | `(x0+9, y0+13)`   | `SPRITE_COEUR` |
| Magic ball icon         | `(x0+9, y0+36)`   | `SPRITE_MAGIE` |
| Coin/Kashes icon        | `(x0+340, y0+15)` | `SPRITE_PIECE` |
| Little-key icon         | `(x0+340, y0+55)` | `SPRITE_CLE` |
| Clover (filled)         | `(x1, y0+58)`     | `SPRITE_FULL_CLOVER_BOX` (loop, x1 from `RegleTrois`) |
| Clover (box)            | `(x1, y0+58)`     | `SPRITE_CLOVER_BOX` |

`CTRL_X1 = 550` means the bar's right edge is **already at 550** — only 90px
shy of the 4:3 right edge. In a wider framebuffer this needs to slide right
or stretch.

### Behaviour-wheel options ([COMPORTE.CPP:187](../../SOURCES/COMPORTE.CPP))

```cpp
x0 = CTRL_X0 + DebComportement + num * 110 - 1;
y0 = CTRL_Y0 + 10 - 1;
```

Five behaviour tiles spread horizontally at 110px stride from `CTRL_X0`.
Stride is hardcoded.

### Dialog speech bubble ([INCRUST.CPP:139-145](../../SOURCES/INCRUST.CPP))

```cpp
SpriteX = Xp + 10;   // or Xp - 10 - DX_BULLE on the other side
SpriteY = Yp - 20;
PtrAffGraph(SpriteX, SpriteY, SpriteBulle);
```

`Xp`/`Yp` are projected from the speaker's world position. **Game-space
anchor** — auto-scales with viewport (no widescreen action needed at the
sprite level; projection handles it).

### Inventory grid ([INVENT.CPP](../../SOURCES/INVENT.CPP))

Multiple sites, all relative to `PLAN_X0/Y0/X1/Y1` (the inventory window
rect, hardcoded in `DEFINES.H`):

- Frame corners ([INVENT.CPP:2467-2485](../../SOURCES/INVENT.CPP)):
  `SYS_SPRITE_SG/IG/ID/SD` at `(x0+2, y0+2)`, `(x0+2, y1-27)`,
  `(x1-27, y1-27)`, `(x1-27, y0+2)`.
- Found-object icon ([INVENT.CPP:806](../../SOURCES/INVENT.CPP)):
  `SYS_SPRITE_INV` centred at `(x0 + (x1-x0)/2, y0 + (y1-y0)/2)`.
- Slate arrows ([INVENT.CPP:1880-1881](../../SOURCES/INVENT.CPP)):
  `SPRITE_ARDOISE_LEFT_UP+on` / `SPRITE_ARDOISE_RIGHT_UP+on` at `(L_X0, L_Y0)`
  and `(R_X0, R_Y0)` (slate margin coordinates).

All relative to a hardcoded UI rect. Widescreen needs the rect re-centred (or
the inventory layout rethought); the sprites follow the rect for free.

### Menu cursor + sprite incrustation ([GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP))

```cpp
// 883:
#define DrawCursor() AffGraph(0, CursorX + 4, CursorY, HQR_Get(HQRPtrSprite, 9));

// 2762-2770:
SpriteX = 10 + PtrZvExtra[MenuIncrustSprite * 8 + 0];
SpriteY = 10 + PtrZvExtra[MenuIncrustSprite * 8 + 1];
// (or PtrZvExtraRaw branch)
PtrAffGraph(SpriteX, SpriteY, MenuIncrustSprite);
```

`CursorX`/`Y` move with selection (game-derived). The menu-incrust sprite
lands at `(10 + zv_offset, 10 + zv_offset)` — a hardcoded top-left
incrustation slot.

### Mouse cursor ([DIRTYBOX.CPP:452](../../LIB386/SVGA/DIRTYBOX.CPP))

```cpp
AffGraph(MouseSpriteGraphicNum, sx, sy, MouseSpriteGraphic);
```

`sx`/`sy` come from `MouseX`/`Y`. Game-space-screen (already scales with
window); no widescreen action.

### Slideshow logo overlay ([GAMEMENU.CPP:4201](../../SOURCES/GAMEMENU.CPP))

```cpp
// AffGraph( 0, 639-105, 3, HQR_Get(HQRPtrSprite,11) ) ;
```

Commented out — the LBA2 logo overlay at `(639-105, 3)` was disabled because
of a palette conflict. The `639 -` anchor is the kind of pattern that
widescreen needs to audit if it ever gets re-enabled.

## Out-of-scope (game-space anchors)

Confirmed via grep that these compute `SpriteX/Y` from 3D projection, not
hardcoded UI:

- [OBJECT.CPP:5011, 5016, 5097-5111, 5169-5181, 5925-5933](../../SOURCES/OBJECT.CPP)
  — all object/sprite blits via projected `Xp/Yp`. Auto-scales with viewport.
- [GRILLE.CPP:739, 901](../../SOURCES/GRILLE.CPP) — isometric brick blits at
  computed `XScreen/YScreen`. Viewport-relative; covered by Phase 5.
- [MESSAGE.CPP:1415](../../SOURCES/MESSAGE.CPP) — typewriter glyph rendering;
  XY computed from dialog box rect (which is hardcoded — see below).

## Hardcoded UI rects (sprite-adjacent, not sprite themselves)

These rectangles drive almost every HUD sprite anchor:

| Define | Value (search [SOURCES/DEFINES.H](../../SOURCES/DEFINES.H) / per-CPP) | Function |
|---|---|---|
| `CTRL_X0/Y0/X1/Y1`     | 100,100 — 550,290 | Behaviour menu |
| `PLAN_X0/Y0/X1/Y1`     | (slate panel rect)| Inventory slate composite (see [INVENT.CPP](../../SOURCES/INVENT.CPP)) |
| `L_X0/Y0`, `R_X0/Y0`   | (slate arrow margins) | Slate left/right arrows |

Widescreen needs these recentred or rescaled (per the projection-correction
phase). Sprites then follow the rect for free.

## Summary

The sprite-as-HUD surface is roughly:

1. **Behaviour-menu info bar** — hardcoded 450×80 rect at (100, 300), six
   sprite slots inside.
2. **Behaviour wheel** — five tiles at 110px stride from `CTRL_X0`.
3. **Inventory window** — frame corners + arrows, all relative to a fixed
   `PLAN_*` rect.
4. **Menu cursor + sprite incrustation** — fixed `(10 + offset)` top-left
   slot, cursor follows selection.
5. **Slideshow logo** — currently disabled; would need a widescreen audit if
   restored.

Everything else (game-world sprite blits, mouse cursor, dialog bubble,
typewriter glyphs) is anchor-derived and auto-scales with the viewport. The
"asset" itself never needs rework — what needs work is the **hardcoded
rectangles** that drive the anchors, which is render-pipeline scope, not
asset scope. The companion catalog confirms there is no SPRITE.HQR /
SPRIRAW.HQR asset-side widescreen work either.
