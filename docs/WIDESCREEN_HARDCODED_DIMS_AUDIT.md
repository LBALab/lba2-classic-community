# Widescreen hardcoded-dim audit

Phase 3 ships a wider render path, but a sweep of the codebase for literals
near 640 and 480 (and their halves, 320 / 240) turns up many sites the
projection-routing PRs (#199–#203) didn't touch. Some corrupt memory at
wider widths; some misalign UI; some are coincidental.

This doc catalogues every match, categorises by impact, and proposes an
ordering for follow-up fix PRs. **No engine changes here** — just the
survey.

## Method

```bash
grep -rEn "[^0-9_a-zA-Z](635-645|475-485|315-325|235-245|288|226|311)[^0-9_a-zA-Z]" \
     SOURCES/ LIB386/ --include="*.CPP" --include="*.C" --include="*.H"
```

Filtered: lines that are pure `//` or `/* … */` comments, lines inside
the `COPY.CPP` / `DEC.CPP` / `DEC_XCF.CPP` / `GRILLE_A.CPP` block-comment
ASM mirrors (those files are entirely wrapped in `/* */` per
`[[project_asm_dead_in_shipping_binary]]`), and trivial coincidences
like font-glyph byte patterns or angle constants.

Total: **~110 distinct sites** across **15 source files**.

## Category A — Critical: corrupts memory / breaks rendering at wider widths

These sites assume the framebuffer is exactly 640×480 in a way that breaks
the renderer when it isn't. They're the priority — they'll be visible
artifacts (or crashes) before any UI mispositioning shows up.

### A1. Pixel-shift macros use 640 as row stride

[`SOURCES/INVENT.CPP:2273-2395`](../SOURCES/INVENT.CPP) — ~24 lines inside
`InventoryShiftPixels*` helpers do raw pixel arithmetic like:

```cpp
dest[5] = src[640];       // 2nd ligne
dest[10] = src[640 * 3];  // 4eme ligne
dest[640] = src[1];       // 2nd ligne, reverse direction
```

The `640 * N` factors are the row pitch of the source/dest buffer. At
768×480 the actual pitch is 768, so every "2nd ligne" read/write lands
128 bytes early — pulling from the wrong row entirely. The inventory's
texture-shift animation will scramble.

### A2. Fixed buffer offset into per-resolution memory

[`SOURCES/INVENT.CPP:2259`](../SOURCES/INVENT.CPP):

```cpp
#define PtrBackupAngles (ScreenAux + 640 * 480)
```

`ScreenAux` is sized `RESOLUTION_X * RESOLUTION_Y + RECOVER_AREA` per
[`MEM.CPP:33`](../SOURCES/MEM.CPP). At 768×480, `640*480 = 307200` lands
*inside* the active pixel region — `PtrBackupAngles` writes corrupt the
scene rendered behind the inventory.

### A3. Z-buffer clear stride

[`SOURCES/HOLOGLOB.CPP:1004`](../SOURCES/HOLOGLOB.CPP):

```cpp
memset(PtrZBuffer + TabOffLine[ZBufYMin], 0xFF,
       (ZBufYMax - ZBufYMin + 1) * 640 * 2);
```

Even with the Z-buffer correctly sized (now `RESOLUTION_X*Y*2` per #205),
this memset uses 640 as the row stride. At 768 the clear leaves the
rightmost 128 columns of each row uncleared — Z-test garbage persists,
holomap renders with stale depth.

### A4. Terrain horizon polygons clamped to Y=479

[`SOURCES/3DEXT/TERRAIN.CPP:570-641`](../SOURCES/3DEXT/TERRAIN.CPP) — six
sites set `Tab_Points[N].Pt_YE = (S16)479` as the bottom edge of the
horizon polygons. If render height ever moves above 480 (future HD), the
horizon stops at row 479. (At the current 480 height this is fine; flag
for the Phase 5 HD pass.)

### A5. Holomap Z-buffer Y bounds

[`SOURCES/HOLOGLOB.CPP:141, 152, 153, 995`](../SOURCES/HOLOGLOB.CPP) —
`ZBufYMin/Max = 479` clamps. Same situation as A4: fine at the current
height, breaks at HD.

### A6. Save-thumbnail buffer

[`SOURCES/SAVEGAME.CPP:457, 502`](../SOURCES/SAVEGAME.CPP):

```cpp
DefFileBufferInit("…/NAME.ME", BufSpeak, 640 * 480);
```

`BufSpeak` is sized at `RESOLUTION_X*Y+RECOVER_AREA`. Init at `640*480`
likely truncates the buffer to a 4:3 region. Save/load thumbnails may
corrupt at wider widths.

### A7. Misc full-framebuffer allocations + sizes

| Site | What |
|---|---|
| [`SOURCES/PERSO.CPP:2008`](../SOURCES/PERSO.CPP) | `DefFileBufferInit(PathConfigFile, ScreenAux, (640 * 480 + RECOVER_AREA))` |
| [`SOURCES/PLAYACF.CPP:222-456`](../SOURCES/PLAYACF.CPP) | Cinematic video buffer at `screenWidth = 320` then `* 480` |
| [`SOURCES/PLAYACF.CPP:504`](../SOURCES/PLAYACF.CPP) | `CopyBlock(0, 0, 640, 480, dest, …)` |
| [`SOURCES/DEFINES.H:211`](../SOURCES/DEFINES.H) | `#define MAX_PLAYER ((640 * 480) / …)` |
| [`SOURCES/CONFIG/MAIN.CPP:90`](../SOURCES/CONFIG/MAIN.CPP) | `#define ibuffersize (640 * 480 + RECOVER_AREA)` |

These mostly affect specific subsystems (config load, video playback,
save thumbnails). Each needs the same `RESOLUTION_X * RESOLUTION_Y`
treatment.

## Category B — Functional: UI misalignment

Doesn't corrupt memory but UI is positioned for 4:3 and misaligns in
widescreen frames. The 3D scene rendering already centres correctly per
Phase 2; these are the 2D UI layer.

### B1. Full-screen shade-box (menu darkening)

`ShadeBoxBlk(0, 0, 639, 479, SCREEN_SHADE_LVL)` — appears **14 times** in
[`SOURCES/GAMEMENU.CPP`](../SOURCES/GAMEMENU.CPP) (lines 1247, 1679,
1708, 1909, 2859, 2915, 2959, 3022, 3095, 3311, 3526, 3672, 3735, 3762,
3814, 4032). At 768×480 these darken only the left 640 columns — the
rightmost 128 columns stay un-shaded behind menus.

### B2. UI text centred at X=320

87 occurrences of `320` as a text-anchor. The big consumers:

| File | Sites |
|---|---|
| [`SOURCES/GAMEMENU.CPP`](../SOURCES/GAMEMENU.CPP) | ~30 — `DrawOneString(320, …)`, `DrawSingleString(320, 240, …)`, etc. |
| [`SOURCES/CONFIG.CPP`](../SOURCES/CONFIG.CPP) | 7 — config-screen text centred at 320 |
| [`SOURCES/SAVEGAME.CPP:544-547`](../SOURCES/SAVEGAME.CPP) | save-prompt text box centred at (320, 240) |
| [`SOURCES/GAMEMENU.CPP:4521-4524`](../SOURCES/GAMEMENU.CPP) | "Saved" message box at (320 ± X, 240 ± 25) |
| [`SOURCES/MESSAGE.CPP:298`](../SOURCES/MESSAGE.CPP) | `Font(320 - dx/2, …, "Please wait")` |
| [`SOURCES/PERSO.CPP:2226-2229`](../SOURCES/PERSO.CPP) | system message at `320 - x, 236` |

All need re-anchoring to `ModeDesiredX/2`.

### B3. Inventory / dialog layout

| File | Site | What |
|---|---|---|
| [`SOURCES/INVENT.CPP:1871`](../SOURCES/INVENT.CPP) | `#define PLAN_X1 (PLAN_X0 + 479)` | Slate panel right edge — slate art is 480 wide, OK as-is |
| [`SOURCES/INVENT.H:23,41`](../SOURCES/INVENT.H) | `INV_DELTA_X = 639 - INV_START_X*2`, `INV_NAME_X1 = 639 - INV_START_X` | Inventory panel width tied to 639 |
| [`SOURCES/INVENT.H:34,36`](../SOURCES/INVENT.H) | `INV_ZOOM_X0 = 317 - 120`, `INV_ZOOM_X1 = 317 + 120` | Zoom box at iso projection X (`317 = 320 - 3`) |
| [`SOURCES/MESSAGE.CPP:110, 111, 1227, 1237, 1247, 1258, 1969, 1970`](../SOURCES/MESSAGE.CPP) | `Dial_X1 = 639 - 16`, `Dial_Y1 = 479 - 16` and variants | Dialog box right/bottom margins |

### B4. Holomap UI

[`SOURCES/HOLOPLAN.CPP:431-433`](../SOURCES/HOLOPLAN.CPP):

```cpp
AffLowerLeft(0, 479, 0);
AffUpperRight(639, 0, 0);
AffLowerRight(639, 479, 0);
```

Holomap corner decorations anchored to 4:3 corners. Also:
- [`HOLOPLAN.CPP:468`](../SOURCES/HOLOPLAN.CPP) — `SetProjection(320, 240, 1024, 700, 700)` — holomap 3D projection centre
- [`HOLOPLAN.CPP:747`](../SOURCES/HOLOPLAN.CPP) — `BoxStaticAdd(0, 0, 639, 479)` — full-screen dirty marker
- [`HOLOPLAN.CPP:808-815`](../SOURCES/HOLOPLAN.CPP) — zoom-in animation bounded by 639/479

### B5. Game-over 3D projection

[`SOURCES/GAMEMENU.CPP:3938`](../SOURCES/GAMEMENU.CPP):

```cpp
SetProjection(320, 240, 128, 200, 200);
```

Game-over model rendered at hardcoded 4:3 centre. The model is a 3D
prop; widening would centre it.

### B6. Fade / venetian-blind transitions

[`SOURCES/GAMEMENU.CPP:4633-4639`](../SOURCES/GAMEMENU.CPP) — fade-in
animation that uses both 479 (row count) and 640 (row stride). Same
class as A1: needs routing.

[`SOURCES/GAMEMENU.CPP:3220-3227`](../SOURCES/GAMEMENU.CPP) —
`ScaleBox(0, 0, 639, 479, Screen, x0, y0, x1, y1, Log)` — save-load
zoom animation, full-screen source rect hardcoded.

### B7. AMBIANCE extended visibility

[`SOURCES/AMBIANCE.CPP:178-188`](../SOURCES/AMBIANCE.CPP):

```cpp
if ((yp > 480 + 480)   OR(yp < -480))
    …
else if ((xp < -320)   OR(xp >= 640 + 320))
    …
```

These checks use 640/480 as visible-extent bounds for ambient effects.
The `480 + 480` and `640 + 320` margins assume specific frame size.

## Category C — Acceptable or coincidental

### C1. Inventory slate art is 480 wide

[`SOURCES/INVENT.CPP:1871`](../SOURCES/INVENT.CPP) `PLAN_X1 = PLAN_X0 +
479` is correct as-is — the slate bitmap is exactly 480 pixels wide
(see [`docs/widescreen/art-catalog.md`](widescreen/art-catalog.md)).
This isn't a framebuffer dim.

### C2. Angle math

`OBJECT.CPP:1718-1754`, `GERELIFE.CPP:169-470`, `OBJECT.CPP:4603, 4626`
— the numbers 320, 384, 640, 896 here are angles (`MUL_ANGLE`-scaled,
where 4096 = full turn). Not pixels.

### C3. Font glyphs

`LIB386/SVGA/AFFSTR.CPP:25-263` — values like 240, 252, 254 in the font
glyph data are bit patterns, not coordinates.

### C4. Dead ASM mirrors

`SOURCES/COPY.CPP`, `SOURCES/DEC.CPP`, `SOURCES/DEC_XCF.CPP`,
`SOURCES/GRILLE_A.CPP`, and the `tests/*_flat.ASM` files all sit inside
`/* */` blocks per [`[[project_asm_dead_in_shipping_binary]]`](../.).
Hits here can be ignored.

### C5. BlitBox 320-wide path

`LIB386/SVGA/BLITBOXF.CPP:17, 38` — `320` here is the 16-byte-chunked
copy stride for a specific intermediate buffer. Audit independently
if the BlitBox path ever widens; for now, it's specific to that buffer's
size.

### C6. Currently-fine 479 references (HD-only concern)

A4, A5 above (`Tab_Points[N].Pt_YE = 479`, `ZBufYMin/Max = 479`) work
at the current 480 render height. They become bugs only when render
*height* changes — Phase 5 HD work.

## Recommended fix ordering

Tracked as the follow-up after #206. Order minimises visible-bug surface
in this priority:

1. **Tier 1 — wider builds corrupt or visibly break.** A1 (`INVENT.CPP`
   row strides), A2 (`PtrBackupAngles`), A3 (Z-buffer clear stride). These
   manifest the moment any widescreen save loads or any inventory opens.
2. **Tier 2 — wider builds misalign UI but render correctly.** B1
   (shade-box), B2 (centred text), B3 (inventory + dialog rect), B4
   (holomap UI), B5 (game-over projection), B6 (fade transitions). Phase
   5 UI work in [`WIDESCREEN.md`](WIDESCREEN.md).
3. **Tier 3 — Misc full-buffer sizing.** A6 (save thumbnails), A7 (config
   buffer, video playback). Each subsystem on its own.
4. **Tier 4 — HD-only concerns.** A4, A5, C6.

## Cross-references

- [`docs/WIDESCREEN.md`](WIDESCREEN.md) — overall plan
- [`docs/WIDESCREEN_PROJECTION_AUDIT.md`](WIDESCREEN_PROJECTION_AUDIT.md)
  — original render-space audit (this doc is the missing UI-space +
  pixel-arithmetic sweep that audit didn't try to cover)
- [`docs/widescreen/sprite-hud-sites.md`](widescreen/sprite-hud-sites.md)
  — sprite/HUD anchor-rect catalog from earlier widescreen research
- #205 (Z-buffer size) and #206 (DrawOverBrick col-roundtrip) — the
  two fixes from the Phase 3 live-demo session that exposed this audit
