# Runtime resolution switching

Design doc for letting the player change render resolution **without relaunching the game** — first via console, then via a Display options submenu.

Supplements [`WIDESCREEN.md`](WIDESCREEN.md) (which covers the underlying widescreen rendering work). Most of the heavy lifting is already done by the existing widescreen campaign — this doc is about wrapping the boot-time `--resolution` infrastructure in a runtime-callable path and a user-facing chooser.

## Status

**Phase 0 is shipped.** Console-driven runtime switch + keep/revert dialog + `lba2.cfg` persistence are live on `main` (PRs #237–#241, #244). What changes vs. this doc's original Phase 0 plan:

- The keep/revert dialog is **not text-mode** as the doc proposed — it's a proper engine-styled panel with a chamfered cadre, vertically-stacked `DrawOneString` buttons, and animated plasma highlight on the focused button. Same visual vocabulary as the in-game menus.
- Persistence is live: `ResolutionX` / `ResolutionY` in `lba2.cfg`, written on dialog Keep, read at boot by `Res_LoadBootDimensions`. Self-heals on bad cfg values (warning logged, falls back to default, next clean exit overwrites the bad keys).
- `--resolution` CLI override still wins over cfg (escape hatch for "I can't see anything, get me back").

See the [Phase status](#phase-status) table at the bottom for what's still ahead.

## Motivation

`--resolution WxH` works at launch and is the right thing for power users running from a terminal. For everyone else:

- Players don't read the CLI flags page.
- "Try 1280×720, didn't like it, want to go back to 640×480" requires quitting + relaunching + remembering the old flag.
- Steam Deck / handheld players don't have a CLI at all.
- Most other engines let you pick from a dropdown — players expect it.

The widescreen render path (Phases 1–3 in `WIDESCREEN.md`) is already runtime-driven: every framebuffer-sized buffer reads `ModeDesiredX/Y` at use-site. The remaining gap is teardown + re-init at runtime, plus a UI to drive it.

## Non-goals

- **Resolution-independent UI.** The 4:3 UI is C1-centred or C2-re-anchored case by case — that work belongs to the audit, not this doc.
- **Per-display preferences.** A single global resolution stored in `lba2.cfg`; if the player moves their laptop to a 4K monitor, they pick a new one. Multi-display memory is a future enhancement.
- **Cinematic upscale.** Smacker source is 320×200. `VideoFullScreen` selects the upscale policy (`Video_ComputeCineRect` in `PLAYACF.CPP`): fit-to-screen (default, aspect-preserving pillar/letterbox) or the classic centred 2× letterbox (PR #430). Render resolution does not change cinematic source quality either way.
- **Display-mode switching.** This is the *render* resolution. SDL scales the framebuffer to the window/display. We don't change the OS display mode.

## Existing infrastructure

What's already in place (cite-checked against current `main`):

| Site | What it does |
|---|---|
| [`SOURCES/MEM.CPP:57`](../SOURCES/MEM.CPP) — `Mem_ConfigureScreenBuffers(resX, resY)` | Sets the framebuffer-sized entries of `ListMem` before the single-shot `InitMainBuffer` allocation. Already called at boot from `PERSO.CPP:2346` once the `--resolution` flag is parsed. |
| [`SOURCES/MEM.CPP:175`](../SOURCES/MEM.CPP) — `InitMainBuffer()` | One `Malloc(SizeOfMemAllocated)` for the entire `MainBuffer` pool, then hands out pointer slices to each `ListMem` entry. Currently fire-and-forget — no teardown. |
| [`LIB386/SVGA/SCREEN.CPP:42`](../LIB386/SVGA/SCREEN.CPP) — `CreateScreenMemory(resX, resY)` | Allocates `Log` (independent of `MainBuffer`), sets `ModeDesiredX/Y`, recomputes `TabOffLine[]`. Reusable: it no-ops the `Log` allocation if non-`NULL`, so we can free + re-call cleanly. |
| [`LIB386/SVGA/SCREEN.CPP:29`](../LIB386/SVGA/SCREEN.CPP) — `EndScreen()` | Frees `Log` and `Screen`. Already exists, currently unused. |
| [`LIB386/SYSTEM/WINDOW.CPP:161`](../LIB386/SYSTEM/WINDOW.CPP) — `CreateWindowSurface(resX, resY)` | SDL window + renderer + streaming texture creation. |
| [`LIB386/SYSTEM/WINDOW.CPP:186`](../LIB386/SYSTEM/WINDOW.CPP) — `DestroyWindowSurface()` | Tears down the SDL texture/renderer/window in the right order. |
| `--resolution WxH` arg parsing | Parses to integers, validates `W % 8 == 0`, range `320×200` – `1920×1080`. Reuse the same validator for the runtime path. |

What's missing:

| Need | Where |
|---|---|
| `Mem_TeardownMainBuffer()` | New, in `MEM.CPP`. Frees `MainBuffer`, nulls every `*ListMem[i].AdrPtr`. |
| Runtime "switch resolution" orchestration | New, probably in a new file `SOURCES/RES_SWITCH.CPP` so the boot path and the runtime path can both call it. |
| Console verb `resolution` | `SOURCES/CONSOLE/CONSOLE_CMD.CPP`. |
| Display options submenu | `SOURCES/GAMEMENU.CPP` — phase 2 work. |
| Preview / revert dialog | Render-and-poll loop with a 15s countdown; lives alongside the menu code. |
| `lba2.cfg` keys | New entries in the config writer/reader. |

## User experience

### Console

```
> resolution
Current: 768x480 (16:10, recommended)

Available:
  1: 640x480   (4:3, classic)        * always present
  2: 768x480   (16:10, recommended)  ← current
  3: 1024x480  (~21:10, recommended)
  4: 1280x720  (16:9, recommended)
  5: 1920x1080 (16:9, recommended)
  6: native    (matches display: 1920x1080)
Advanced (--all): list 30+ additional W%8==0 sizes.

Usage:
  resolution <n>           pick from the list above
  resolution <WxH>          arbitrary, e.g. resolution 800x480
  resolution native         match the player's display
  resolution --all          show every supported width
```

- `resolution` with no args: print the numbered list + current selection.
- `resolution <n>`: switch to entry `<n>`.
- `resolution WxH`: arbitrary explicit. Goes through the same validator as `--resolution`.
- `resolution native`: SDL_GetCurrentDisplayMode → snap to nearest `W % 8 == 0` ≤ display.
- `resolution --all`: print the advanced list (all multiples of 8 from 320 to 1920, all heights from 200 to 1024, filtered to `≤ display`).
- Switch applies immediately; opens the preview / revert dialog.

### Menu — Display options (Phase 2)

Add a **Display options** entry alongside Controls in the existing options menu. Selecting it opens a self-contained modal — the same UX model as the save/load slot picker (`SAVEGAME.CPP`), so we don't have to wedge a scrollable list into the parent options screen.

```
┌── Display options ──────────────────────────┐
│ → 640x480    (4:3, classic)                 │
│   768x480    (16:10, recommended)           │
│   1024x480   (~21:10, recommended)          │
│   1280x720   (16:9, experimental)           │
│   1920x1080  (16:9, experimental)           │
│   ----------                                │
│   Advanced...                               │
│   Fullscreen: off                           │
│   Vsync: on                                 │
│ ◄ Back                                      │
└─────────────────────────────────────────────┘
```

- Same list source as the console verb (computed once, shared).
- "Advanced..." opens a longer scrollable view with every filtered option.
- Selecting an entry triggers the preview / revert dialog.
- Fullscreen and Vsync are siblings — same submenu, different settings. They don't gate the resolution choice; they apply independently.

### Preview / revert dialog

Modern OSes give you ~15s to confirm a display change. Without it, picking 1920×1024 on a 720p display makes the game unreadable and the player is stuck. This is V1, not a future enhancement.

```
┌── Resolution changed ──────────────┐
│                                    │
│ Now running at 1280×720.           │
│                                    │
│ Keep this resolution?              │
│                                    │
│ [Y] Keep      [N] Revert           │
│                                    │
│ Reverts automatically in 12s...    │
│                                    │
└────────────────────────────────────┘
```

- Renders at the **new** resolution (so we know the new mode is at least drawing something).
- 15-second countdown; default action is revert.
- Any input that isn't Y/Enter reverts.
- Revert path reuses the same `Res_Switch()` orchestrator with the previous (W, H) values.

If the new mode failed to allocate (out of memory) or SDL refused to create the texture, the switch is aborted before the dialog ever appears — the old resolution is restored and an error is logged.

## Resolution catalog

### Tier definitions

| Tier | Definition |
|---|---|
| **Classic** | Exactly `640×480`. Pinned at the top of every list. Never filtered out. The authentic-mode anchor. |
| **Recommended** | A small fixed list of common aspect ratios, filtered to `≤ display`. Today's list: `640×480` (4:3), `768×480` (16:10), `1024×480` (~21:10), `1280×720` (16:9 — experimental), `1920×1080` (16:9 — experimental), plus a synthetic `Native` entry resolved to `display_W × display_H` snapped to `W % 8 == 0`. |
| **Advanced** | A curated list of common modes (320×200 … 1680×1024), filtered to `≤ display`. Shown behind `resolution --all` so the default list isn't dozens of entries. |

> **Menu vs console.** The two tiers above (`Res_BuildCatalog`) back the **console** `resolution` verb: a curated, power-user list plus arbitrary `WxH`. The **in-game menu** uses a different builder, `Res_BuildMenuList`: the `640×480` classic anchor plus a ladder of **display-aspect-matched** modes at 480/720/1080 render heights, computed per monitor. Every menu entry therefore fills the player's exact panel (no letterbox/pillarbox). For example, a 16:9 panel offers `640×480, 848×480, 1280×720, 1920×1080`; a 16:10 panel offers `768×480, 1152×720, 1728×1080`; an odd-aspect laptop gets its own widths. The `@480` rung equals the boot auto-detect value, so the first-launch resolution is always a listed (and markable) entry. Rungs that don't fit the panel, or whose width would exceed 1920 (ultrawide at 1080), are dropped.

### Aspect labeling

Display in the form `<W>x<H> (<aspect>, <tag>)`:

- `aspect` is `gcd(W,H)` reduction → `16:9`, `16:10`, `21:9`, `4:3`, etc.
- `tag` is one of `classic`, `recommended`, `experimental`, or `advanced` per tier and per gating rules.
- Mark current selection with `←` (or in console output, `← current`).

### Filter rules

| Rule | Reason |
|---|---|
| `W % 8 == 0` | [`LIB386/H/SYSTEM/RESOLUTION.H`](../LIB386/H/SYSTEM/RESOLUTION.H) static check; filler step accumulators are tuned for it. |
| `320 ≤ W ≤ 1920` | Matches `--resolution` CLI validator. |
| `200 ≤ H ≤ 1024` | Same. |
| `W ≤ display_W` and `H ≤ display_H` | Don't offer modes larger than the display; SDL would still scale, but it's misleading. |
| `Y > 480` flagged `experimental` | A4 (terrain horizon) and A5 (holomap Z-buffer Y bounds) are now fixed (routed through `ModeDesiredY-1`, PR #262), as is the actor-shadow cull (#443). Tall modes stay flagged `experimental` for the remaining Phase 7 polish rather than correctness bugs: HUD and menu fonts blit 1:1 (tiny at 1080p), and the `ui holoplan` tall-res path is still unverified. The player can still pick them; we tell them what to expect. |

### SDL discovery

Use `SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window))` to get the player's current display dimensions. This is the `display_W × display_H` used in the filters and the `native` entry.

**Not** `SDL_GetFullscreenDisplayModes()`. That's the list of modes you can switch the display *itself* into — irrelevant here. The framebuffer is a render target SDL scales to the window/display; the resolution choice is the render resolution, not the display mode.

## Safety gates

The runtime switch is gated on game state being in a "safe to tear down framebuffers" mode. Refuse with a reason if any of these are true:

| Gate | Reason |
|---|---|
| In cinematic / Smacker playback (`FlagPlayAcf`) | Mid-decode; `dest` buffer state lost on teardown. |
| In dialogue (`Dial_Letterbox` / `FlagDial`) | Letterbox state mid-render. |
| In inventory (`FlagInvent`) | `ScreenAux` holds backup-angle state. |
| In holomap (`HoloMode != -1`) | Per-globe / per-plan state in transient buffers. |
| At a save-cancel or quit-confirm modal | Modal stack would lose its anchor. |

Implementation: a single static `Res_SwitchAllowedReason()` predicate that walks the flags and returns either `NULL` (allowed) or a short user-facing reason. Both console and menu paths check it; menu greys the resolution list with a one-line tooltip; console prints "Not now: in cinematic — finish the cutscene first" and returns.

## Persistence

`lba2.cfg` gets two new keys:

```
ResolutionX=768
ResolutionY=480
```

Read at boot: if both keys are present and valid, they seed the initial render resolution. `--resolution WxH` on the CLI overrides. If both are missing, default to `640×480`.

Write: on resolution-switch success (after the keep-confirm dialog), write the new values back to `lba2.cfg`. Same mechanism as the existing `WriteConfigFile` in `PERSO.CPP:2008`.

Vsync persists as a `VSync=0|1` key in `lba2.cfg`, read at boot in `ReadConfigFile` and applied via `Window_SetVSync` (defaults on; no boot-flash to avoid, so no early pre-read like fullscreen needs). The Fullscreen toggle persists via the existing `DisplayFullScreen` key. Both are live as Display-submenu rows today, alongside a `vsync on|off|toggle` console verb.

## Phase status

| Phase | Scope | Status |
|---|---|---|
| **Phase 0** | `Mem_TeardownMainBuffer` + `DestroyVideoSurface`. `Res_Switch(newW, newH)` orchestrator with in-place resize (no destroy/recreate of the SDL window — fullscreen state survives). `cmd_resolution` console verb (list / select by number / arbitrary `WxH` / `native` / `--all`). Safety gate (`Res_SwitchAllowedReason`). Engine-styled keep/revert dialog: panel + chamfered cadre + two vertically-stacked `DrawOneString` buttons + animated plasma on the focused button. 15 s timeout, default focus on Revert. `lba2.cfg` `ResolutionX`/`Y` persistence — written on dialog Keep, read at boot by `Res_LoadBootDimensions` (CLI > cfg > default precedence). Self-heals on invalid cfg via warning + write-default-on-exit. Four harness tests cover orchestrator, console verb, dialog timeout, and cfg precedence. | ✅ shipped (#237–#241, #244) |
| **Phase 1** | Boot-path consolidation: today `PERSO.CPP main` and `INITADEL.C` both call `Res_LoadBootDimensions` and `Mem_ConfigureScreenBuffers + InitMainBuffer` happens before `InitGraphics`. The "given W, H, set everything up" sequence is partially deduplicated via the shared resolver, but the surrounding init flow could be tighter. Pull the lot into one re-entrant function callable from both the boot path and `Res_Switch`. | not started |
| **Phase 2** | Display options submenu in `GAMEMENU.CPP` — `MENU_ID_DISPLAY` entry at the top of OptionsMenu (above Sound volume). Opens a self-contained submenu (`DisplayMenu[]`) modelled on `LanguageMenu`: one cycling "Resolution: <current>" row plus Back. Left/right (or Enter) cycles between Classic 4:3 (640×480) and a Widescreen entry computed from the current display's aspect ratio via `Window_GetDisplayDimensions` (snapped to `W%8==0` at 480 height). The cycle calls `Res_Switch` directly and persists to `lba2.cfg` via `WriteConfigFile` — no keep/revert dialog on this path, since the two-entry curated list is inherently safe (the Widescreen entry can't be larger than the player's display). Catalog helpers live in `SOURCES/RES_MENU.{H,CPP}`. `ui display <path>` capture verb + golden lock the rendering. Advanced list / Fullscreen / Vsync siblings deliberately deferred — keeps v1 to two entries until A4/A5 unblocks HD modes. The console `resolution` verb still serves power users wanting arbitrary widths (and still uses the keep/revert dialog because those choices can be unsafe). | ✅ shipped (this PR) |
| **Phase 3** | Polish: keep music position across switch (save / restore stream pos), restore mouse cursor cleanly across the resize, tighten the safety-gate UI feedback. Fullscreen / Vsync persistence is done: the Vsync row + `vsync` console verb + `VSync` cfg key shipped, joining the already-live `DisplayFullScreen` key. Music continuity is still V1-acceptable to glitch today; the rest are quality-of-life. | partial |

## Open questions

- **Music continuity** across the switch — V1 is the "glitch on switch" behaviour as planned (the music track stops/restarts naturally as the scene re-init runs `ChoicePalette` and friends). Phase 3 saves stream position.
- **Linear vs nearest texture scaling** — `WINDOW.CPP` ships `SDL_SCALEMODE_NEAREST` (pixel-art). A future Display submenu toggle could expose `LINEAR` for "soften the upscale"; not V1.
- **Per-display memory** — single global cfg today (`ResolutionX` / `ResolutionY` in `lba2.cfg`). If the player frequently swaps monitors, they might want per-display memory. Out of scope for V1.
- **Boot-flash at non-default cfg resolution** — *resolved.* Shape B (transient `DefFileBufferInit` against a static 8 KB BSS buffer in `Res_LoadBootDimensions`) reads the cfg before `Mem_ConfigureScreenBuffers`, so MainBuffer is sized correctly at the cfg dimensions in one shot. No boot-time `Res_Switch` cycle.

## Risks (in retrospect, after Phase 0)

1. **Pointer staleness through `MainBuffer`.** *Confirmed real, fixed.* Several subsystems cached pointers into MainBuffer slices at scene-init time — `ListPartFlow[i].PtrListDot` indexes into `ListFlowDots`; `PtrNuances`/`PtrCLUTFog`/`PtrCLUTGouraud`/`PtrPalCurrent`/`PtrTransPal` all derive offsets from `PtrXplPalette`. `Res_Switch` re-runs `InitPartFlow` and `ChoicePalette` after rebuild to rewire them. Plus `ClipWindow` needed to be set *after* `ResizeVideoSurface` (the `ModeResX` clamp happens against the video-surface size, not the Log size); `XCentre`/`YCentre` needed `PtrInit3DView` + `CameraCenter(0)` for interior iso scenes (`Init3DView` resets the camera to origin); `FirstTime = AFF_ALL_FLIP` to invalidate the dirty-box state. All caught by manual testing because headless `SDL_VIDEODRIVER=dummy` papered over half of them — see [`SOURCES/RES_SWITCH.CPP`](../SOURCES/RES_SWITCH.CPP) comments.
2. **SDL hot-swap order matters.** *Avoided.* The first cut destroyed + recreated the SDL window which would have flickered the compositor and dropped focus in fullscreen. Pivoted to in-place `SDL_SetWindowSize` + texture-only recreate; window object survives, fullscreen state survives, multi-monitor placement survives.
3. **Cfg write fails after switch.** *Handled.* `WriteConfigFile` is best-effort; failure logs to `adeline.log` but doesn't block the player. The runtime switch already worked for this session; persistence just won't survive a relaunch. Manual recovery is `--resolution WxH` on the CLI.
4. **Test coverage gap.** *Closed.* Four harness tests cover the surface: `test_res_switch` (orchestrator across 5 modes), `test_res_switch_console` (4 verb forms + 2 error paths), `test_res_switch_dialog` (15s timeout → auto-revert chain), `test_res_switch_cfg` (cfg precedence + invalid-value fallback, isolated via `XDG_DATA_HOME`).

Net: the Phase 0 plan held up well — the actual scope landed close to estimate. The hidden iceberg was the cached-pointer story in (1), which needed live-on-engine testing because the headless harness couldn't see it. Documented in code.

## Out of scope (explicit non-features)

- Changing the OS display mode (fullscreen switching changes the SDL window flag, not the display).
- Hot-reloading textures / palettes from disk on switch.
- Animating the transition (we render a black frame during teardown).
- Cross-resolution save compatibility (already works — `MAX_PLAYER` is keyed to `640×480` per the save-format invariant in `WIDESCREEN.md`).
