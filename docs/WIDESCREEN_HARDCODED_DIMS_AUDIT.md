# Widescreen hardcoded-dim audit

Phase 3 ships a wider render path, but a sweep of the codebase for literals
near 640 and 480 (and their halves, 320 / 240) turns up many sites the
projection-routing PRs (#199–#203) didn't touch. Some corrupt memory at
wider widths; some misalign UI; some are coincidental.

This doc catalogues every match, categorises by impact, and proposes an
ordering for follow-up fix PRs. **No engine changes here** — just the
survey.

## A note on what counts as a "fix"

Two distinct classes of hit hide behind a literal `640`:

- **Row-stride / pointer arithmetic** that addresses a buffer (Log,
  ScreenAux, the Z-buffer): the `640` is meant to be the buffer's row
  pitch or end-of-pixels offset. At any wider buffer it's wrong — these
  are real bugs that need the literal routed through the buffer's
  actual width, regardless of any UI architecture decision.
- **2D UI layout coordinates** (text anchors, panel right edges, full-
  screen darken rects): the `640` is the *authored canvas width* the
  UI was designed for. Whether to route these through `ModeDesiredX`
  depends on the UI-strategy choice from [`WIDESCREEN.md` Phase 5](WIDESCREEN.md#ui-strategy-c1-vs-c2):
  - **C1** = render the 4:3 UI letterboxed into a centred 4:3 region.
    Keep the `640` / `639` / `320` literals — they're correct for the
    authored layout. The wide framebuffer just frames the UI with
    margins, like the SCREEN.HQR letterbox treatment from
    [`docs/widescreen/art-strategy.md`](widescreen/art-strategy.md).
  - **C2** = re-anchor UI to scale. Route the literals through
    `ModeDesiredX/Y` so the UI fills the wider frame.

This audit doesn't pick C1 vs C2 — it surfaces what each choice would
need to touch. The **row-stride bugs are bugs under either choice** and
should land first.

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

**True bugs under either C1 or C2.** These sites use `640` as a buffer
row stride or as an end-of-pixels offset; the framebuffer is now wider
so the literal is wrong regardless of how the UI is laid out.

### A1. Pixel-shift macros use 640 as Log row stride

[`SOURCES/INVENT.CPP:2273-2395`](../SOURCES/INVENT.CPP) — ~24 lines inside
`BackupAngles` / `RestoreAngles` (the dialog/inventory corner-rendering
save/restore) do raw pixel arithmetic like:

```cpp
src = (U8 *)Log + TabOffLine[y0] + x0;   // src points INTO Log (framebuffer)
*(U32 *)dest = *(U32 *)src;              // 1ere ligne
dest[5] = src[640];                      // 2nd ligne  ← assumes Log row pitch = 640
dest[10] = src[640 * 3];                 // 4eme ligne ← same
```

The `640 * N` factors are the row pitch of `Log` (the back-buffer being
read). At 768-wide Log, `src[640]` lands at column `x0 + 640` on the
*same* row — wrong source pixels copied into the backup, garbage on
restore. **This is a bug under both C1 and C2**: even with a letterboxed
4:3 UI, the source pointer still indexes the wider Log buffer; one row
down is `ModeDesiredX` bytes forward, never literal 640.

Fix: replace the `640` row stride with `TabOffLine[1]` (or equivalent
`(S32)ModeDesiredX`). The destination side stays as-is — `dest` is a
small backup region with its own tight stride, unrelated to Log.

### A2. Fixed buffer offset into per-resolution memory

[`SOURCES/INVENT.CPP:2259`](../SOURCES/INVENT.CPP):

```cpp
#define PtrBackupAngles (ScreenAux + 640 * 480)
```

`ScreenAux` is sized `RESOLUTION_X * RESOLUTION_Y + RECOVER_AREA` per
[`MEM.CPP:33`](../SOURCES/MEM.CPP). The intent of the `640 * 480` offset
is "past the end of active pixels" — into the `RECOVER_AREA` tail. At
768×480 the active pixels extend to byte 368639; `640 * 480 = 307200`
is *inside* them. Writes corrupt the scene behind the inventory.

Bug under both C1 and C2 — the macro wants to land past whatever the
framebuffer-pixel area actually is. Fix: `ScreenAux + ModeDesiredX *
ModeDesiredY` (or `RESOLUTION_X * RESOLUTION_Y` if compile-time is fine).

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

### A7. Iso AffBrickBlock left clip assumes hotX=-24

[`SOURCES/GRILLE.CPP:738`](../SOURCES/GRILLE.CPP) — `AffBrickBlock` was
gated on `XScreen >= -24`. The threshold matches iso bricks with
`hotX = -24` (half-brick centered), but many iso bricks have `hotX = 0`
or `+24`. Their visible pixel range at `XScreen = -32` is `[0, 15]` —
16 pixels on-screen — yet they were rejected.

The historical 4:3 hid this. The iso origin offset is
`ModeDesiredX/2 - 32 = 288 = 12 × 24` at 640, and the integer iso-diagonal
`x - z` always lands on `XScreen` values that are exact multiples of 24
relative to that offset — so no iso column ever falls in the "visible
but rejected" `[-47, -24)` strip. At 768 the offset is `352 = 14 × 24 + 16`;
the iso diagonal `x - z = -16` projects to `XScreen = -32` and the
left-edge wall column drops.

The bug pops on any width where `(ModeDesiredX/2 - 32) % 24 != 0` —
that's 768, 800, 1280, 1366, 1920 (and similar). It silently hides on
640, 1024, 1600 (all multiples of 48). Same projection-correct /
consumer-broken pattern as A1/A2/A3 above and as the prior
[T_COLONB col round-trip fix (#206)](../SOURCES/GRILLE.CPP).

Fix: relax to `XScreen >= -47` and skip `ListBrickColon` bucketing for
`XScreen < -24` (col index would go negative otherwise — DrawOverBrick
just won't re-blit the leftmost pixel column, acceptable edge case).
`AffBrickBlockColon` (the mask-only sibling used by labyrinth mode at
GRILLE.CPP:820) keeps `>= -24` — relaxing it would be a no-op since
it can't bucket negative-col bricks anyway.

### A8. DrawOverBrick col-scan miss for south walls

[`SOURCES/GRILLE.CPP:593`](../SOURCES/GRILLE.CPP) — `DrawOverBrick` is the
per-object re-blit that paints bricks in front of an actor *over* the
actor's already-drawn pixels (so a wall correctly occludes the lower body
of a character standing behind it). It scans the per-col `ListBrickColon`
buckets covering the actor's screen bounding box:

```cpp
startcol = (ClipXMin + 24) / 24 - 1;   // historical
endcol   = (ClipXMax + 24) / 24;
```

The `-1` was meant to cover one extra col to the left so that a wall at
the actor's iso-cube *south* neighbour (which projects to
`XScreen = actor_iso_XScreen - 24`, i.e. `col = actor_iso_col - 1`) is
included.

That `-1` only works when the actor's iso center lands exactly on a
col-bucket boundary — true at widths where the iso origin offset
`(ModeDesiredX/2 - 32)` is a multiple of 24. At 640 that's
`288 = 12 × 24`, so the actor's clip box left edge and the wall-south
col line up such that `col(ClipXMin) - 1 == wall_south_col`. At 768 the
offset is `352 = 14 × 24 + 16` — actor centers slide off the boundary,
`col(ClipXMin) - 1` ends up *in the same col as the actor*, and the
south-wall col is missed entirely. The character is then drawn on top
of walls he should be behind.

Same `(offset % 24 != 0)` alignment class as `A7` and the prior
[T_COLONB col round-trip fix (#206)](../SOURCES/GRILLE.CPP). Confirmed
empirically: instrumented log of 874 hero-`DrawOverBrick` calls at 768
fired `CopyMask` exactly **0** times for walls south of the hero; an
equivalent narrow run fired it 287 times across 82 calls.

Fix: widen the bias to `-2` and clamp `startcol >= 0`. The extra col is
a harmless scan when empty; when populated, the per-brick
`recouvre`/`infront` filter (world-cube coords, alignment-independent)
keeps wrong walls out of the re-blit. `DrawOverBrick3` and
`DrawOverBrickCage` use the same `-1` formula but apply to different
object classes (sprites and the end-of-game cage) — not yet observed to
manifest the visual bug at any width, so left alone pending evidence.

### A9. Misc full-framebuffer allocations + sizes

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

## Category B — Functional: UI layout architecture choice

**Architecture-dependent.** These sites are 2D UI layout coords authored
at 640×480. Whether to route them through `ModeDesiredX/Y` depends on
the C1/C2 choice — see [`WIDESCREEN.md` Phase 5](WIDESCREEN.md#ui-strategy-c1-vs-c2):

- **Under C1 (letterbox the 4:3 UI):** *leave the literals alone.* The
  fix lives elsewhere — composite the rendered 4:3 UI into the centre
  of the wider Log. Most of these sites become a no-op.
- **Under C2 (re-anchor UI):** route each literal through
  `ModeDesiredX/Y`. The list below is the surface area.

The 3D scene rendering already centres correctly per Phase 2; these are
purely the 2D UI layer.

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

1. **Tier 1 — true bugs, independent of UI strategy.** A1 (`INVENT.CPP`
   row strides), A2 (`PtrBackupAngles`), A3 (Z-buffer clear stride). These
   corrupt memory or render garbage at any wider build, no matter what
   UI strategy is chosen. Land first.
2. **Tier 1.5 — A6/A7 buffer sizing.** Save-thumbnail buffer, config
   loader, cinematic video. Subsystem-specific fixes; can run in parallel
   with Tier 1.
3. **C1 vs C2 decision** — pick the UI strategy before touching Category
   B. The bulk of Category B (~80 sites) is either a no-op or the whole
   surface depending on this choice; don't litigate it site-by-site.
4. **Tier 2 — UI layout per the chosen strategy.** Category B sites.
   Under C1: build a 4:3-render-into-wider-Log compositor. Under C2:
   route the literals.
5. **Tier 4 — HD-only.** A4 (terrain horizon Y=479), A5 (holomap
   ZBufYMax=479), C6. Fine at 480 height; defer.

The author's intuition that "the inventory is 4:3 and should stay that
way" maps cleanly to C1 — and is consistent with the SCREEN.HQR
letterbox treatment from [`docs/widescreen/art-strategy.md`](widescreen/art-strategy.md).
That call would also retire most of Tier 2 from this audit by making
the literals correct-as-authored.

## Cross-references

- [`docs/WIDESCREEN.md`](WIDESCREEN.md) — overall plan
- [`docs/WIDESCREEN_PROJECTION_AUDIT.md`](WIDESCREEN_PROJECTION_AUDIT.md)
  — original render-space audit (this doc is the missing UI-space +
  pixel-arithmetic sweep that audit didn't try to cover)
- [`docs/widescreen/sprite-hud-sites.md`](widescreen/sprite-hud-sites.md)
  — sprite/HUD anchor-rect catalog from earlier widescreen research
- #205 (Z-buffer size) and #206 (DrawOverBrick col-roundtrip) — the
  two fixes from the Phase 3 live-demo session that exposed this audit
