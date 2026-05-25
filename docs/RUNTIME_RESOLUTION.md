# Runtime resolution switching

Design doc for letting the player change render resolution **without relaunching the game** — first via console, then via a Display options submenu.

Supplements [`WIDESCREEN.md`](WIDESCREEN.md) (which covers the underlying widescreen rendering work). Most of the heavy lifting is already done by the existing widescreen campaign — this doc is about wrapping the boot-time `--resolution` infrastructure in a runtime-callable path and a user-facing chooser.

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
- **Cinematic upscale.** Smacker source is 320×200; at higher render resolutions it stays letterboxed inside the 640×480 image area (see `PLAYACF.CPP`). Render resolution does not change cinematic source quality.
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
| `--resolution WxH` arg parsing | Parses to integers, validates `W % 8 == 0`, range `320×200` – `1920×1024`. Reuse the same validator for the runtime path. |

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
| **Advanced** | All multiples of 8 in width from 320 to 1920, all heights from 200 to 1024, filtered to `≤ display`. Shown behind an "Advanced..." gate so the Recommended list isn't 60 entries. |

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
| `Y > 480` flagged `experimental` | Until A4 (terrain horizon) and A5 (holomap Z-buffer Y bounds) are fixed (audit row in `WIDESCREEN_HARDCODED_DIMS_AUDIT.md` — task #99 / HD pass), tall modes glitch on the holomap. The player can still pick them; we tell them what to expect. |

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

Fullscreen and Vsync would land as separate `lba2.cfg` keys (`Fullscreen=0|1`, `Vsync=0|1`) in Phase 3, not V1.

## Phases

| Phase | Scope | Estimate |
|---|---|---|
| **Phase 0** | `Mem_TeardownMainBuffer()`. `Res_Switch(newW, newH)` orchestrator. `cmd_resolution` console verb (list / select by number / arbitrary `WxH` / `native` / `--all`). Safety gates. **Preview / revert dialog rendered in console-mode** (text-based, 15s timeout, Y/N input). Test that exercises console-driven switch via the harness. | 2–4 days |
| **Phase 1** | Boot-path refactor: consolidate the duplicated init sequence so the boot path and `Res_Switch` share the same orchestration. Today the boot does `Mem_ConfigureScreenBuffers → InitMainBuffer → InitAdeline → CreateScreenMemory → InitMemory` — pull the "given W, H, set everything up" body into one function callable from both. | 1 day |
| **Phase 2** | Display options submenu in `GAMEMENU.CPP` — list with the same source the console uses, "Advanced..." sub-list, Fullscreen and Vsync siblings. Reuses the Phase 0 preview/revert dialog (rendered as a proper menu modal this time, not text). | 1–2 days |
| **Phase 3** | Polish: keep music position across switch (save / restore stream pos), restore mouse cursor cleanly, persist Fullscreen / Vsync to `lba2.cfg`, tighten the safety-gate UI feedback. | 1 day |

**Total V1**: 5–8 days.

## Open questions

- **Music continuity** across the switch — V1 acceptable to glitch (stop / restart current track), Phase 3 saves stream position. Worth confirming before Phase 0.
- **Linear vs nearest texture scaling** — `WINDOW.CPP:176` is `SDL_SCALEMODE_NEAREST` (pixel-art). A future Display submenu toggle could expose `LINEAR` for "soften the upscale"; not V1.
- **Per-display memory** — single global cfg today. If the player frequently swaps monitors, they might want per-display memory. Out of scope for V1.

## Risks

1. **Pointer staleness through `MainBuffer`.** `ListMem` is a single-shot pool. Several subsystems (inventory backup, holomap arrows, etc.) cache pointers into the buffers at scene-init time. Safety gates handle most of this by refusing the switch in those states; the rest must be exercised by tests.
2. **SDL hot-swap order matters.** `DestroyWindowSurface` before `EndScreen` before `Mem_TeardownMainBuffer`, then the inverse order on rebuild. Easy to get wrong; well-defined enough to encode in `Res_Switch`.
3. **Cfg write fails after switch.** Resolution applied, dialog confirmed, but `WriteConfigFile` errors out — player gets the new mode this session, loses it next launch. Log it; not blocking.
4. **Test coverage gap.** Today's `--resolution` tests cover boot-time. Runtime-switch needs its own harness test: load save, run `resolution 768x480` via the console-exec path, verify the scene re-renders correctly at the new width. New `tests/automation/test_resolution_switch.sh`.

## Out of scope (explicit non-features)

- Changing the OS display mode (fullscreen switching changes the SDL window flag, not the display).
- Hot-reloading textures / palettes from disk on switch.
- Animating the transition (we render a black frame during teardown).
- Cross-resolution save compatibility (already works — `MAX_PLAYER` is keyed to `640×480` per the save-format invariant in `WIDESCREEN.md`).
