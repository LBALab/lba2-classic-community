# Sprites

The sprite system covers three different drawing surfaces:

1. **Screen-space UI** — inventory icons, HUD, menu cursors, console glyphs. Drawn 1:1 from the `SPRITES.HQR` bank via `PtrAffGraph` / `AffGraph`.
2. **World-space "extras"** — magic ball, projectiles, dropped pickups (kashes, hearts, magic refills, keys, clovers), particle pofs, foudre. Drawn through the sort tree alongside 3D objects, with a per-frame perspective scale factor.
3. **Animated 3D sprites** — door frames, lifts, scenery animations driven by the `ANIM_3DS` flag. Drawn 1:1 (no scaling) but participate in sort-tree depth ordering.

For terms like "cube", "extra", "pof", "labyrinthe", see [GLOSSARY.md](GLOSSARY.md). For the surrounding render flow, see [SCENES.md](SCENES.md) and the `AffScene()` / sort-tree summary in [project memory: rendering pipeline].

## Storage

Two HQR resources hold the pixel data:

| Variable | Resource | Used for |
| --- | --- | --- |
| `HQRPtrSprite`    | `SPRITES.HQR`   | UI / HUD sprites (drawn unscaled by `PtrAffGraph` → `AffGraph`) |
| `HQRPtrSpriteRaw` | `SPRIRAW.HQR`   | World-space sprites (drawn by `ScaleSprite` / `ScaleSpriteTransp`) |

Loaded by `PERSO.CPP:2535-2549` via `HQR_Init_Ressource`, with budgets `SpriteMem` / `SpriteRawMem` (`MEM.CPP`).

The dispatch is by sprite index: anything with index `>= 100` is a "UI" sprite (`HQRPtrSprite`), anything below is a "raw" world sprite (`HQRPtrSpriteRaw`). See `INTEXT.CPP:295-301` (`GetPtrSprite`).

Per-sprite bounding-volume metadata lives in two parallel S16 arrays loaded from the resource HQR:

- `PtrZvExtraRaw[sprite * 8 + i]` — for raw (world) sprites, indices 0/1 are the on-screen hot-spot offsets `(mindx, mindy)`, indices 2–7 are the world AABB `(XMin, XMax, YMin, YMax, ZMin, ZMax)`.
- `PtrZvExtra[sprite * 8 + i]`     — same layout for UI/effect sprites.
- `PtrZvAnim3DS[sprite * 8 + i]`   — same layout for `ANIM_3DS`-flagged objects.

`InitSprite` (`OBJECT.CPP:2354`) copies the AABB onto the `T_OBJET` when an object's sprite changes.

### Sprite header (`Struc_Sprite`)

Every entry in a sprite bank starts with a 4-byte header (`LIB386/SVGA/SCALESPI.CPP:11-16`):

```c
typedef struct {
    U8 Delta_X;   // pixel width
    U8 Delta_Y;   // pixel height
    S8 Hot_X;     // signed hot-spot offset, applied at draw time
    S8 Hot_Y;
} Struc_Sprite;
```

followed by `Delta_X * Delta_Y` palette-indexed bytes (color 0 = transparent). A sprite bank is a `U32` offset table at the start (`tab[num]` is the byte offset of sprite `num`'s header), used by both renderers.

The hot-spot is what the engine places at `(x, y)`: `screen_x = x + Hot_X`. In the scaled blitter, the hot-spot itself is scaled by `factorx` so the visual centre of the sprite stays put as the sprite shrinks.

## Render primitives

Three blit primitives. All three live in `LIB386/SVGA/`, declared in `LIB386/H/SVGA/`, and write to the `Log` framebuffer using `TabOffLine` / `ModeDesiredX`. Clipping uses `ClipXMin` / `ClipXMax` / `ClipYMin` / `ClipYMax`. They report the touched bounding box back via `ScreenXMin` … `ScreenYMax`.

### `AffGraph` — opaque, unscaled

(`LIB386/SVGA/AFFSTR.CPP`, used via `PtrAffGraph` in `SOURCES/INTEXT.CPP:285`.) Straight memcpy-style blit with color-0 transparency. Used for UI sprites and `ANIM_3DS` objects in the world.

### `ScaleSprite` — scaled, opaque

`LIB386/SVGA/SCALESPI.CPP`. Same algorithm as `ScaleSpriteTransp` (the only ASM-equivalent reference) — the inner pixel write copies the source byte straight to the framebuffer (color 0 = transparent sentinel, all others written as-is) instead of blending through `PtrTransPal`. Two paths:

- **Fast path** (`factorx == 65536 && factory == 65536`): straight blit with color-0 transparency, hot-spot offsets from the sprite header.
- **Scaled path**: 16.16 fractional walk over the destination rectangle. Hot-spot, top-left, bottom-right and texel start are derived through `mul_shift16(value, factor)` and `sprite_center_16_16(delta)` so a centred sprite stays centred when shrunk.

`factorx <= 0` follows the ASM convention: `0` returns exit-bounds, negative is treated as `+INT_MAX` (sprite collapses to a single texel column / row).

History note: the C++ port shipped `factorx`/`factory`-ignoring code from commit `e0da0c8` (Sept 2025) until the scaled paths were ported back from `ScaleSpriteTransp`. See [Magic ball — distance scaling](#magic-ball--distance-scaling) below.

### `ScaleSpriteTransp` — scaled, transparency-table

`LIB386/SVGA/SCALESPT.CPP:39-255`. This one **does** scale, in two paths:

- **Fast path** (`factorx == 65536 && factory == 65536`): 1:1 blit but with per-pixel transparency-table blending. Each source pixel `s` and destination pixel `d` index into `transpTable[(s << 8) | d]` to produce the final value. The 256×256 table is `PtrTransPal` (loaded with the level palette).
- **Scaled path**: traverses the destination rectangle with 16.16 fixed-point texel coordinates. Increments `xFrac += xDec` per pixel and `yFrac += yDec` per row, where `xDec = (1/factorx) << 16` (computed via `divide_16_16`). Hot-spot, top-left, bottom-right and texel start are all derived through `mul_shift16(value, factor)` and `sprite_center_16_16(delta)` — the latter returns `(delta/2) << 16` (rounded up by ½ when `delta` is odd) so a centred sprite stays centred when shrunk.

Both paths report the on-screen bounding box back through `Screen{X,Y}{Min,Max}` and bail out via `set_exit_bounds()` when the sprite is fully outside the clip rect.

## Per-frame scale factor

`ScaleFactorSprite` (`SOURCES/GLOBAL.CPP:207`, default `DEF_SCALE_FACTOR = 65536` from `COMMON.H:420`) is the 16.16 zoom factor handed to the scaled blitter. There are two ways it gets set:

- **Forced 1:1** (UI, HUD, interior with `Scale = -1`): callers explicitly set `ScaleFactorSprite = DEF_SCALE_FACTOR;` before drawing. See `OBJECT.CPP:5096` (anim3DS path), `OBJECT.CPP:5943` (`AffScene` UI overlay), `GAMEMENU.CPP:2519`, `COMPORTE.CPP:266`, `INTEXT.CPP:249` (`PtrProjectSprite` for sprite index ≥ 100).
- **Perspective from world Y delta** (`SOURCES/EXTFUNC.CPP:352-372`):

```c
void CalculeScaleFactorSprite(S32 x, S32 y, S32 z, S32 scaleorg) {
    if (CubeMode == CUBE_INTERIEUR AND scaleorg == -1) {
        ScaleFactorSprite = DEF_SCALE_FACTOR;
    } else {
        S32 yp0;

        LongWorldRotatePoint(x, y, z);
        if (!LongProjectPoint(X0, Y0 + 1000, Z0))
            return;
        yp0 = Yp;
        if (!LongProjectPoint(X0, Y0, Z0))
            return;

        if (scaleorg <= 0)
            scaleorg = 70;

        ScaleFactorSprite = ((Yp - yp0) * DEF_SCALE_FACTOR) / scaleorg;
    }
}
```

In words: project the sprite's anchor point and a second point 1000 world units above it, take the on-screen Y difference (`Yp - yp0`, in pixels), and scale `DEF_SCALE_FACTOR` by `(Yp - yp0) / scaleorg`. A sprite far from the camera projects with a small Y delta, so `ScaleFactorSprite` shrinks; a sprite up close gets a large Y delta and scales up. `scaleorg` is the sprite's natural Y delta (defaults to 70) — pick `70` so the on-screen height matches the original art at "reference" distance.

Two failure modes worth noting:

- If either `LongProjectPoint` fails (point behind the near plane / off the projection volume), the function returns without updating `ScaleFactorSprite`. The previous frame's value (or the last caller's value) leaks into this draw — usually harmless because the sprite is off-screen, but worth knowing.
- `Scale = -1` + `CUBE_INTERIEUR` short-circuits to 1:1, which is the original behavior: interior scenes are isometric, so projection wouldn't give a meaningful perspective scale anyway.

## Integration with the sort tree

Each in-world extra is inserted into the sort tree by `AffScene` (`OBJECT.CPP:5640-5680`):

```c
GetExtraZV(ptrextra, &txmin, ..., &tzmax);   // world AABB from PtrZvExtraRaw
txmin += ptrextra->PosX; ...                  // shift to world space

TreeInsert((S16)(n | TYPE_EXTRA),
           x, y, z,                           // anchor (sort/projection)
           txmin, tymin, tzmin,
           txmax, tymax, tzmax);
```

The dispatch back out is in `AffOneObject` (`OBJECT.CPP:5157-5200`):

```c
case TYPE_EXTRA:
    ptrextra = &ListExtra[numobj];
    PtrProjectPoint(ptrextra->PosX, ptrextra->PosY, ptrextra->PosZ);

    num = ptrextra->Sprite;
    if (num & 32768) {
        AffSpecial(numobj);                   // pof / 3D body / labyrinthe brick
    } else {
        SpriteX = Xp + PtrZvExtraRaw[num*8 + 0];
        SpriteY = Yp + PtrZvExtraRaw[num*8 + 1];

        CalculeScaleFactorSprite(ptrextra->PosX, ptrextra->PosY, ptrextra->PosZ,
                                 ptrextra->Scale);

        if (ptrextra->Flags & EXTRA_TRANSPARENT) {
            ScaleSpriteTransp(0, SpriteX, SpriteY,
                              ScaleFactorSprite, ScaleFactorSprite,
                              HQR_Get(HQRPtrSpriteRaw, num),
                              PtrTransPal);
        } else {
            ScaleSprite(0, SpriteX, SpriteY,
                        ScaleFactorSprite, ScaleFactorSprite,
                        HQR_Get(HQRPtrSpriteRaw, num));
        }
    }
```

`SpriteX` / `SpriteY` are the projected anchor in screen space, plus the per-sprite hot-spot offset (`PtrZvExtraRaw[num*8+0..1]`). The blitter then adds the sprite-header `Hot_X` / `Hot_Y` on top — these are conceptually distinct: the bbox offset positions the sprite as a whole, and the header hot-spot anchors *within* the sprite (and is what gets scaled by `factorx`).

The high bit on `Sprite` (`num & 32768`) reroutes to `AffSpecial` (`EXTRA.CPP:773-831`), which dispatches by `Sprite & 32767`:

| Sprite & 0x7FFF | Renderer |
| --- | --- |
| 0 | `PofDisplay3DExt` — particle pof, growing/shrinking ring |
| 3 | `BodyDisplay_AlphaBeta` — full 3D body (e.g. a thrown weapon model) |
| 4 | `AffOneBrick` — a single labyrinthe brick |

These bypass the sprite blitters entirely.

## Magic ball

The magic ball is the player's primary attack: a sprite projectile that reacts to magic level, magic point count, and whether a key is pending.

### Throw — `ThrowMagicBall` (`SOURCES/FICHE.CPP:52-160`)

Tables in `FICHE.CPP:24-37` map `MagicLevel` (0–4) to:

```c
U8 MagicBallHitForce[] = { DEGATS_BALLE_LVL_01, DEGATS_BALLE_LVL_01,
                           DEGATS_BALLE_LVL_2,  DEGATS_BALLE_LVL_3,
                           DEGATS_BALLE_LVL_4 };
U8 MagicBallSprite[]   = { SPRITE_BALLE_LVL_01, SPRITE_BALLE_LVL_01,
                           SPRITE_BALLE_LVL_2,  SPRITE_BALLE_LVL_3,
                           SPRITE_BALLE_LVL_4 };
```

`ThrowMagicBall` then derives `MagicBallType` from `MagicPoint`:

| `MagicBallType` | Condition | Behavior |
| --- | --- | --- |
| 0 | `MagicPoint == 0` | straight, no bounce |
| 1 | `MagicPoint` 1–20 | bounces up to `MagicBallCount = 4` times |
| 2–4 | `MagicPoint` 21–80+ | collapsed to type 1 in the switch (4 bounces) — the type field is rewritten to 1 |
| 5 | `SearchBonusKey() != -1` | homing, tracks the key — uses `ExtraSearchKey` instead of `ThrowExtra` |

Each branch calls `ThrowExtra(NUM_PERSO, x, y, z, sprite, alpha, beta, vitesse, poids, force)` which finds a free `T_EXTRA` slot, sets `Sprite = num`, `Scale = -1`, and the flag combo `EXTRA_END_OBJ | EXTRA_END_COL | EXTRA_IMPACT`.

After the throw, `ThrowMagicBall` ORs in `EXTRA_MAGIC_BALL` so `GereExtras` recognizes it. At `MagicLevel == 4` it adds `EXTRA_TRANSPARENT` and spawns three trail extras (`SPRITE_TRAINEE_BALLE_1+n`) with explicit `Scale` values (`75 + 15 * n`) and staggered timers — these render through `ScaleSpriteTransp`.

Lastly it spends a magic point: `if (MagicPoint > 0) MagicPoint--;`.

### Update — `GereExtras` (`SOURCES/EXTRA.CPP:1660+`)

A long per-frame loop over `ListExtra[]` (`MAX_EXTRAS` slots). The magic ball hits the `EXTRA_FLY` branch:

```c
time = (TimerRefHR - ptrextra->Timer) / 20;
ptrextra->PosX = ptrextra->Vx * time + ptrextra->U.Org.X;
ptrextra->PosY = ptrextra->Vy * time + ptrextra->U.Org.Y
                 - (ptrextra->Poids * time * time) / 16;   // gravity
ptrextra->PosZ = ptrextra->Vz * time + ptrextra->U.Org.Z;
```

so trajectory is `org + V*t - (g*t²)/16`. Per-axis movement is then capped to one brick per tick (no tunneling) and the position is clamped to the cube interior.

Going out of cube bounds — or `MagicBallFlags & MAGIC_BALL_RAPPELEE` — triggers `InitBackMagicBall`, which spawns the return-trip extra (`EXTRA_SEARCH_OBJ`, target = the hero) and plays `SAMPLE_EXPLO_MAGIC_BALL`.

Wall collision (`EXTRA_END_COL`) routes by `MagicBallType`:

- Type 1 with bounces remaining: `BounceExtra`, `SAMPLE_CHOC_MAGIC_BALL`, `NewMagicBallRebond = 3` (kicks the trail in sync), `MagicBallCount--`.
- Type 1 last bounce: `InitBackMagicBall(BACK_SOUND | BACK_EXPLO)`, `ExtraLabyrinthe`, `Sprite = -1`.
- Default: same explode + return.

Object hit (`EXTRA_END_OBJ`): explodes and returns. The trail extras (`EXTRA_TRAINEE`) follow the active ball's velocity and origin with a small delay (`(time - (Body+1)*20) / 20`), re-syncing on bounce via `NewMagicBallRebond`.

`InitBackMagicBall` (`EXTRA.CPP:1629-1657`) sets `MagicBall` to a new search extra and `InitMagicBall = TRUE`, which makes `GereExtras` skip that slot for the current pass (so the ball doesn't process twice in the frame it's converted). The flag is cleared at the bottom of `GereExtras`.

### Render — magic ball draw path

The active magic ball is just another `T_EXTRA`, so it goes through `TreeInsert(TYPE_EXTRA)` like any other extra and ends up in the `case TYPE_EXTRA` branch of `AffOneObject` (above). Of note:

- `ptrextra->Scale = -1` (set by `ThrowExtra`).
- In **interior** scenes, `CalculeScaleFactorSprite` short-circuits to `DEF_SCALE_FACTOR` (1:1) — by design, isometric scenes don't want perspective shrinkage.
- In **exterior** scenes, `CalculeScaleFactorSprite` runs the projection branch — `scaleorg <= 0` so it picks the default `70`, and `ScaleFactorSprite` becomes the perspective-derived value. **This is the value that should make the ball shrink as it flies away.**

## Magic ball — distance scaling

Historical bug, fixed: in retail, the magic ball shrinks as it moves away from the camera; in this port between commit `e0da0c8` (Sept 2025) and the SCALESPI scaled-path restoration, it did not.

The trace lands on the `case TYPE_EXTRA` dispatch in `OBJECT.CPP:5174-5187`. `CalculeScaleFactorSprite` computes a perspective-correct `ScaleFactorSprite` for exterior scenes; the factor is then passed to **either** `ScaleSpriteTransp` **or** `ScaleSprite`, depending on the `EXTRA_TRANSPARENT` flag.

| Path | Affected sprites |
| --- | --- |
| `EXTRA_TRANSPARENT` set → `ScaleSpriteTransp` | Level-4 fire ball only — `ThrowMagicBall` only sets the flag for `MagicLevel == 4` |
| Otherwise → `ScaleSprite` | Magic levels 0–3, plus all opaque world extras (kashes, hearts, magic refills, keys, clovers, …) |

The bug: `ScaleSpriteTransp` always implemented the scaled path; `ScaleSprite` did not. Commit `e0da0c8` ("fix exterior sprites like magicball") had collapsed the C++ implementation from 375 lines to 39, keeping only the 1:1 fast path. `tests/SVGA/test_scalespi.cpp` only ever called at `0x10000, 0x10000`, so CI was silent. The shipped binary calls the C++ stub (the `.ASM` is excluded from the active CMakeLists; see [project memory: ASM is dead in shipping binary]).

### Fix

`ScaleSprite` now mirrors `ScaleSpriteTransp`'s structure verbatim — same negative-factor handling, same fast-vs-scaled split, same fractional walk — with the inner write changed from `*lineDst = transpTable[(s<<8)|d]` to `*lineDst = srcPixel`. The scaled path is byte-for-byte equivalent to the original `SCALESPI.ASM`, validated by `tests/SVGA/test_scalespi.cpp` against `asm_ScaleSprite` at non-unit factors (`0x18000` / `0x14000` scale up, `0x08000` / `0x0C000` scale down, negative-factor and zero-factor edge cases, plus 32 randomized cases per run).

In-game repro of the now-fixed bug: stand in any exterior cube, throw the magic ball at magic level 0–3 — before the fix, the sprite kept its size as the ball flew away; after, it shrinks with distance.

## Summary

- World sprites flow through `AffScene` → sort tree → `AffOneObject` → `ScaleSprite` / `ScaleSpriteTransp`, with `ScaleFactorSprite` set by `CalculeScaleFactorSprite` from a 1000-unit Y projection delta.
- UI sprites bypass the scale factor (`PtrAffGraph` → `AffGraph`).
- Both `ScaleSprite` and `ScaleSpriteTransp` implement matching fast (1:1) and scaled (16.16) paths. They differ only in the per-pixel write: opaque direct copy vs. transparency-table blend.
