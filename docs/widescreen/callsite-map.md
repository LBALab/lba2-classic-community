# Widescreen art callsite map

Inverse view of the [art catalog](art-catalog.md): every code path that pulls
a full-screen bitmap out of `SCREEN.HQR`, grouped by call site, with the
surrounding draw flow noted. Use this as the surface area to audit when
changing render resolution.

Companion to the catalog — read it first for the "what" of each entry.

## Loader primitive

```cpp
// LIB386/SYSTEM/HQRLOAD.CPP:42
U32 Load_HQR(const char *name, void *ptr, S32 index);
```

`ptr` is the destination buffer; for bitmap entries it must be ≥ 640×480 =
307200 bytes, which all the call sites here satisfy (passing `Log`, `Screen`,
`PalettePcx`, or `BufSpeak`). The entry index addresses a single entry — the
caller is responsible for also loading the palette at `index + 1`.

For `EffectPcx(numpcx, …)`, `numpcx` is the *pair index* (i.e. raw entry / 2),
doubled internally:

```cpp
// SOURCES/GAMEMENU.CPP:4597
numpcx *= 2 ;   // car il y a les palettes intercalées
```

## Call sites

### Boot splash chain — `DistribLogo()` / `ShowLogo()` / `BumperFullscreen()`

[GAMEMENU.CPP:4398-4429 ShowLogo()](../../SOURCES/GAMEMENU.CPP) is the shared
single-bitmap presenter:

```cpp
Load_HQR(tmpFilePath, Log, numscr);
Load_HQR(tmpFilePath, PalettePcx, numscr + 1);
BoxStaticFullflip();          // mark whole 640×480 dirty
FadeToPal(PalettePcx);        // fade up
// wait nsec or input
FadeToBlack(PalettePcx);
```

Callers and their entries:

| Caller | Entry loaded | Notes |
|---|---|---|
| [GAMEMENU.CPP:4437](../../SOURCES/GAMEMENU.CPP) `DistribLogo()` (`#ifdef DEMO`) | `3*2 = 6` (`PCR_ARDOISE`) | Likely a stale demo stub — entry 6 is the slate |
| [GAMEMENU.CPP:4443](../../SOURCES/GAMEMENU.CPP) `DistribLogo()` | `PCR_ACTIVISION` (72) | US versions |
| [GAMEMENU.CPP:4447](../../SOURCES/GAMEMENU.CPP) `DistribLogo()` | `PCR_EA` (74) | EA version |
| [GAMEMENU.CPP:4452](../../SOURCES/GAMEMENU.CPP) `DistribLogo()` | `PCR_VIRGIN` (76) | Virgin versions |
| [GAMEMENU.CPP:4494, 4581](../../SOURCES/GAMEMENU.CPP) `BumperFullscreen()` | `PCR_BUMPER` (2) | LBA2 bumper card |

[GAMEMENU.CPP:4463 BumperFullscreen()](../../SOURCES/GAMEMENU.CPP) and
[GAMEMENU.CPP:4551 BumperFullscreenWithPal()](../../SOURCES/GAMEMENU.CPP) each
load `PCR_BUMPER + PCR_BUMPER+1` directly (not via `ShowLogo`) with the same
full-flip fade flow.

[GAMEMENU.CPP:4318 DisplayLogo()](../../SOURCES/GAMEMENU.CPP) loads `PCR_LOGO`
straight into `Log` with its palette into `PalettePcx`, then `BoxStaticFullflip`
+ `FadeToPal`. Same shape, different entry point.

All splash paths assume the 640×480 bitmap fully covers the framebuffer.
Widescreen needs a centred-blit / letterbox at every one of these.

### Main menu backdrop — `PCR_MENU`

Four sites all load `PCR_MENU` (4) as the cloudy-sky background that the
menu UI sits on top of:

| Site | Target buffer | Surrounding code |
|---|---|---|
| [GAMEMENU.CPP:2375](../../SOURCES/GAMEMENU.CPP) `MainGameMenu()` opening | `Screen` → `CopyScreen(Screen, Log)` | Initial menu paint |
| [GAMEMENU.CPP:2486](../../SOURCES/GAMEMENU.CPP) | `Log` → `CopyScreen(Log, Screen)` + `BoxStaticAdd(0,0,ModeDesiredX-1,ModeDesiredY-1)` | Repaint between menu transitions |
| [GAMEMENU.CPP:3444](../../SOURCES/GAMEMENU.CPP) `RestoreGameMenu()` | `Screen` → `BoxStaticFullflip` + `FadeToPal` | Save/load menu backdrop |
| [GAMEMENU.CPP:4133](../../SOURCES/GAMEMENU.CPP) (`if (!mode)`) | `Screen` → `CopyScreen(Screen, Log)` + `BoxStaticFullflip` | Conditional repaint |

The menu text and option box-static decorations are drawn *over* this. For
widescreen we either letterbox the bitmap or extend it (the sky is
horizontally extendable — see strategy doc TBD).

### Slate inventory — `PCR_ARDOISE` + `ListArdoise[]`

The slate is the only path that loads bitmaps into a non-Log/Screen buffer.

[INVENT.CPP:2120](../../SOURCES/INVENT.CPP):
```cpp
Load_HQR(tmpFilePath, Screen, ListArdoise[CurrentArdoise] * 2);   // map content
```

[INVENT.CPP:2147](../../SOURCES/INVENT.CPP):
```cpp
Load_HQR(tmpFilePath, Log, PCR_ARDOISE);   // blank slate frame
CopyScreen(Log, BufSpeak);                 // stash a clean copy
```

`ListArdoise[]` is a 5-slot inventory of acquired slate indices, seeded with
`{5, 7, 9}` at [INVENT.CPP:530-532](../../SOURCES/INVENT.CPP) and extended at
runtime by script opcode [GERELIFE.CPP:2215](../../SOURCES/GERELIFE.CPP). The
`* 2` conversion means slot value `5` → entry `10` = `PCR_AEGOUT` (sewer
slate-grid). The seeded set is sewer / labyrinth / moon.

After loading both, [INVENT.CPP:2120-2160](../../SOURCES/INVENT.CPP) composites
the map content from `Screen` onto the blank slate from `BufSpeak` via
`CopyBlock(PLAN_X0, PLAN_Y0, PLAN_X1, PLAN_Y1, …)`. The slate frame is fixed
640×480; the map content is also full-screen but only a sub-rectangle gets
blitted to the final slate. Widescreen: the slate frame extends well or
letterboxes well; the map sub-rect is content-aware.

### Script-driven cinematics — `EffectPcx` / `EffectPcxMess`

Generic single-bitmap presenter at
[GAMEMENU.CPP:4592 EffectPcx()](../../SOURCES/GAMEMENU.CPP):

```cpp
numpcx *= 2;                                         // pair → raw entry
BoxReset();
Load_HQR(SCREEN.HQR, PalettePcx, numpcx + 1);
switch (effect) {
case PCX_FADE:    Load_HQR(SCREEN.HQR, Log, numpcx); BoxStaticFullflip();
                   FadeToPal(PalettePcx); break;
case PCX_V_SHADE: Load_HQR(SCREEN.HQR, screen, numpcx);
                   PaletteSync(PalettePcx);
                   // venetian-blind row dissolve into Log
                   for (y = 0..479) { CopyBlock(...); BoxStaticAdd(...);
                                      WaitVideoSync(); BoxUpdate(); } break;
}
```

Callers:

- [GAMEMENU.CPP:4020, 4031](../../SOURCES/GAMEMENU.CPP) — inside a `/* */`-
  commented-out `Help()`. Dead code; references undefined `PCR_HELP`.
- [GERELIFE.CPP:2137, 2147](../../SOURCES/GERELIFE.CPP) — script opcode reads
  `num` and `effect` from `*PtrPrg++` bytecode, then calls `EffectPcx` or
  `EffectPcxMess`. Every unnamed cinematic still (entries 26-58, 62-70) lands
  through this one path.
- [GAMEMENU.CPP:4662](../../SOURCES/GAMEMENU.CPP) `EffectPcxMess()` wrapper
  for cinematic stills with an overlaid text message.

Implication: there is exactly one bottleneck (`EffectPcx`) that draws every
script-driven cinematic still. Centre-blit / letterbox here would cover the
whole "Twinsen's adventure cutscenes" surface in one place.

### Disc-check fallback — `PCR_CDROM`

Loaded into `Log` from three sites:

- [MESSAGE.CPP:592](../../SOURCES/MESSAGE.CPP) — followed by `BoxUpdate()` +
  `FadeToPal(PtrPalNormal)`.
- [CONFIG.CPP:1444](../../SOURCES/CONFIG.CPP) — followed by
  `BoxStaticFullflip()`.
- [PERSO.CPP:2432](../../SOURCES/PERSO.CPP) — followed by `BoxStaticFullflip()`,
  inside a Windows CD-scan delay path.

Identical "load and full-flip" shape. Widescreen-safe with a centred blit.

## Coverage gaps in this map

- `GERELIFE.CPP` script opcode 2137/2147 covers most cinematic stills, but the
  exact entry an opcode loads depends on per-scene bytecode. We don't have a
  static map of "which scene's life-script calls EffectPcx with which num."
  Building that would mean parsing `LIFE.HQR` bytecode — TBD.
- Found-object cinematic (the rotating 3D item) is a different render path
  (3D, not bitmap-from-`SCREEN.HQR`) and is not in scope here.
- Holomap, dialog box, found-object are bitmap-composite UI but draw from
  `RESS.HQR` / `SPRITE.HQR`, not `SCREEN.HQR`. Out of scope for this map.

## Summary surface area

To convert every `SCREEN.HQR` presentation to widescreen-safe, the patch sites
are:

1. `ShowLogo()` (one)
2. `DisplayLogo()` (one)
3. `BumperFullscreen()` / `BumperFullscreenWithPal()` (two)
4. Four `PCR_MENU` loaders in `GAMEMENU.CPP`
5. Slate composite in `INVENT.CPP` (two `Load_HQR` calls feeding a `CopyBlock`)
6. `EffectPcx()` — single point covering all script cinematic stills
7. Three `PCR_CDROM` loaders

Eight distinct functions, ~twelve call sites. Centralising the centre-blit /
letterbox into one helper (e.g. `BlitFullscreenBitmap(buf, ptr)`) would touch
all of them.
