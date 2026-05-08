# LBA_EDITOR (PERSO) in source code

This document captures what the `LBA_EDITOR` build paths represent in this repository, based on current source code.

## Scope and interpretation

- `LBA_EDITOR` is a compile-time mode that enables many "tool" code paths.
- Comments repeatedly reference the historical tool executable (`PERSO.EXE` / `PERSO.C`).
- In this fork, these paths are mostly preserved as engine-side behaviors and helpers, not as a complete standalone editor product.

## Relation to DEBUG.md

This document and `docs/DEBUG.md` cover different layers:

- `docs/DEBUG.md`: user-facing/debuggable features in modern builds (primarily `DEBUG_TOOLS`).
- `docs/LBA_EDITOR.md`: historical `LBA_EDITOR` source branches, including legacy tooling hooks that are mostly not active in standard builds.

Use `DEBUG.md` when you want to run or test available debug features today. Use this document when you need to understand legacy editor-oriented code paths and data handling preserved in source.

## High-level model

- **Runtime-first tooling:** The editor logic piggybacks on the game runtime (world, render, camera, object systems) with extra input/debug/edit hooks.
- **Per-subsystem conditionals:** Behavior changes are spread through `#ifdef LBA_EDITOR` in many modules instead of one editor framework.
- **Text-to-binary authoring flow:** Several systems support tool-time text definitions that are compiled/translated for runtime use.

## Capability map by subsystem

### 1) 3D world interaction and picking

Primary files:
- `SOURCES/INTEXT.H`
- `SOURCES/INTEXT.CPP`
- `SOURCES/EXTFUNC.H`
- `SOURCES/EXTFUNC.CPP`

What is implemented:
- Brick/polygon picking from screen coordinates:
  - `PtrGetScreenBrick()` supports interior picking via `GetScreenBrick()` and exterior picking via `SelectPoly()`.
- Mouse-driven edit interactions:
  - `GereBrickMouse()` handles click actions in interior and exterior.
  - Right click can reposition Twinsen onto selected brick/poly.
  - Repeated selection can trigger `GereBrickMenu()` (brick menu entry point callsite).
- Exterior mouse camera motion:
  - `GereMouseMovements()` rotates camera while mouse button is held.
- Mode-specific key handling dispatch:
  - `PtrGereKeys()` routes to interior (`GereSpecialKeys`) or exterior (`GereExtKeys`) key logic.

Likely missing/not fully present:
- The full brick-menu implementation and broader editor UI flow are not centralized in one preserved editor app entrypoint.

### 2) Camera and visualization tools

Primary files:
- `SOURCES/EXTFUNC.CPP`
- `SOURCES/INTEXT.CPP`

What is implemented:
- Rich exterior camera tuning in `GereExtKeys()`:
  - camera distance, offsets, gamma, fog range (`StartZFog`, `ClipZFar`), and related redraw/reinit steps.
- Axis gizmo:
  - `DrawRepereXYZ()` draws local XYZ reference in a viewport rectangle.
- Camera diagnostics panel:
  - `DrawCameraInfos()` prints camera/light/projection values in a window.
- Visualization toggles via `FlagInfos` checks:
  - horizon, camera info, reference overlays, frame-speed mode interactions.

Likely missing/not fully present:
- A documented modern mapping of all `FlagInfos` bit assignments is not obvious in current docs; behavior is visible at usage points.

### 3) Exterior cube transition zone authoring helper

Primary files:
- `SOURCES/EXTFUNC.CPP`

What is implemented:
- Automated creation of exterior transition zones:
  - `CreateExtZone()`, `CreateExtZones()`, `MenuCreateExtZones()`.
- Tool menu allows selecting N/S/E/W edge zones and height, then emits zone entries into `ListZone`.
- Uses cube-name lookup (`SearchNumCube()`) from definition data.

Likely missing/not fully present:
- This is a focused helper, not a complete zone authoring suite with broader map-edit UX.

### 4) Impact scripting and compilation

Primary files:
- `SOURCES/IMPACT.H`
- `SOURCES/IMPACT.CPP`

What is implemented:
- Tool-mode parser/compiler pipeline for impact scripts:
  - `GetImpCommand()`, `CompileImpact()`.
- Command set includes:
  - `SAMPLE`, `FLOW`, `THROW`, `THROW_POF`, `THROW_OBJ`, `SPRITE`, `FLOW_SPRITE`, `FLOW_OBJ`, `FLOW_POF`.
- Name resolution and auto-append helper for `.def`-style files:
  - `GetNumDef()`, `GetNumDefAddIfNot()`.
- Runtime dispatch differs in editor mode:
  - `DoImpact()` uses name-based lookup in tool mode.

Likely missing/not fully present:
- The full external impact-authoring UI/toolchain around these functions is not represented as a complete standalone application here.

### 5) Flow (particle) asset authoring path differences

Primary files:
- `SOURCES/FLOW.H`
- `SOURCES/FLOW.CPP`

What is implemented:
- Tool-mode flow loading from `flow.def` + `.FLW` files (`InitPartFlow()` editor branch).
- Runtime mode loads flow resource data from packed resource/HQR path.
- Editor branch includes additional guards/messages for out-of-range flow definitions.

Likely missing/not fully present:
- The external "makeflow" style tooling process is referenced in comments but not reconstructed as a documented modern workflow.

### 6) Data model extensions for editor mode

Primary files:
- `SOURCES/COMMON.H`
- `SOURCES/COMPORTE.CPP`
- `SOURCES/INVENT.CPP`

What is implemented:
- Several structs gain editor-only fields to preserve authoring metadata:
  - `T_ZONE` has editor-only `Snap` for Visu3D editing.
  - `T_EXTRA` stores impact name string in tool mode.
  - behavior structures include name/path fields (`Name`, `File3d`) for editor usage.
- String-based anim/body matching appears in editor paths where game mode uses numeric IDs.

Likely missing/not fully present:
- A single canonical doc of editor-only struct semantics does not yet exist outside code comments.

### 7) Input and interaction behavior differences

Primary files:
- `SOURCES/INPUT.H`
- `SOURCES/INPUT.CPP`
- `SOURCES/CONFIG/INPUT.CPP`

What is implemented:
- Tool mode adds mouse click gating in input wait logic:
  - `WaitNoClick`, `InitWaitNoClick`, click-aware `WaitNoInput()` / `WaitInput()`.
- Editor paths are integrated into regular input polling (`MyGetInput()`), not a separate event loop.

Likely missing/not fully present:
- No separate dedicated editor input profile system is evident beyond these compile-time branches.

## Verified call paths in current source

These are concrete call flows visible in the current codebase.

- Mouse click to world edit action:
  - `MyGetInput()` updates `MyClick` in tool mode (`SOURCES/INPUT.CPP`, `SOURCES/CONFIG/INPUT.CPP`).
  - `GereBrickMouse()` handles selection/reposition/menu behavior (`SOURCES/INTEXT.CPP`).
  - Exterior clicks can call `DrawGrilleFeuille3D()` and update hero position from selected poly.
- Interior/exterior key dispatch for tool controls:
  - `PtrGereKeys()` selects `GereSpecialKeys()` (interior) vs `GereExtKeys()` (exterior) (`SOURCES/INTEXT.CPP`).
  - `GereExtKeys()` applies camera/fog/offset changes and redraws (`SOURCES/EXTFUNC.CPP`).
- Exterior visualization overlays:
  - `RedrawSpeedTerrain()` checks `FlagInfos` and conditionally draws axis/camera info (`SOURCES/EXTFUNC.CPP`).
- Zone helper path:
  - `MenuCreateExtZones()` -> `CreateExtZones()` -> `CreateExtZone()` -> modifies `ListZone` / `NbZones` (`SOURCES/EXTFUNC.CPP`).
- Impact script compilation path:
  - `CompileImpact()` loops on `GetImpCommand()` and emits binary entities (`SOURCES/IMPACT.CPP`).
  - Tool-mode `DoImpact(name, ...)` resolves by impact name (`SOURCES/IMPACT.CPP`).
- Flow loading split:
  - Tool mode: `InitPartFlow()` reads `flow.def` and `.FLW` files.
  - Game mode: `InitPartFlow()` loads flow resources through HQR resource path (`SOURCES/FLOW.CPP`).

## Console-adopted debugging capabilities

The current console work intentionally carries over only a subset of `LBA_EDITOR`-adjacent capabilities that directly help runtime debugging:

- Visualization toggles that map to existing runtime state (for example ZV and related draw/debug overlays where globals already exist).
- Scene navigation and status workflows (cube jump/status style inspection).
- Inventory/state helpers intended for debugging sessions (`give`, bug snapshots).

## Not adopted in this milestone

The following remain out of scope for the console initiative:

- Legacy editor UI/menu flows (such as `GereBrickMenu`-style authoring interaction).
- Toolchain/authoring pipelines (`flow.def`/impact compiler text authoring paths).
- Reconstructing a full standalone historical editor shell.

## Known gaps and unresolved points

- Several `FlagInfos` / `OptionsDebug` symbols are still unresolved in this fork because their numeric bit definitions are not present in tracked headers.
- The complete historical UI/application shell around `GereBrickMenu()` is not clearly reconstructed as one standalone, modern editor entrypoint.
- Legacy authoring paths (`PATH_FLOW`, `PATH_IMPACT`) are preserved, but the original production pipeline around them is only partially represented.
- Build intent is unclear: this document describes preserved `LBA_EDITOR` paths in source, not a guaranteed supported user-facing target in current presets.

## FlagInfos and OptionsDebug reference

The code contains many checks against `FlagInfos` and `OptionsDebug` in tool-only branches. In this fork, some flag names are used but their numeric bit assignments are not declared in visible tracked headers.

`FlagInfos` flags used by `LBA_EDITOR` code paths:

| Flag symbol | Meaning in practice (from usage) | Bit value found in tracked source? | Source references |
|---|---|---|---|
| `INFO_HORIZON` | Draw horizon overlay in tool visualization paths | Not found | `SOURCES/EXTFUNC.CPP`, `SOURCES/INTEXT.CPP` |
| `INFO_CAMERA` | Show camera diagnostics (`DrawCameraInfos`) | Not found | `SOURCES/EXTFUNC.CPP` |
| `INFO_REPERE` | Draw XYZ reference gizmo (`DrawRepereXYZ`) | Not found | `SOURCES/EXTFUNC.CPP`, `SOURCES/INTEXT.CPP` |
| `INFO_FRAME_SPEED` | Use fast frame-speed visualization path | Not found | `SOURCES/INTEXT.CPP` |
| `INFO_BOX_FLOW` | Flow particles rendered as boxes instead of points | Not found | `SOURCES/FLOW.CPP` |
| `INFO_DRAW_ZV` | Draw bounding volume (`ZV`) debug visuals | Not found | `SOURCES/FLOW.CPP`, `SOURCES/BEZIER.CPP` |
| `INFO_VISU_BOX` | Draw screen-space bounding rectangle for flow debug | Not found | `SOURCES/FLOW.CPP`, `SOURCES/OBJECT.CPP` |
| `INFO_DECORS` | Toggle decor visualization in exterior terrain path | Yes (`1024`) | `SOURCES/3DEXT/DECORS.CPP`, `SOURCES/3DEXT/TERRAIN.CPP` |

`OptionsDebug` flags referenced by `LBA_EDITOR` code paths:

| Flag symbol | Meaning in practice (from usage/comments) | Bit value found in tracked source? | Source references |
|---|---|---|---|
| `OPT_COL_ANGLE` | Emit collision-angle diagnostic message in exterior movement logic | Not found | `SOURCES/EXTFUNC.CPP` |
| `OPT_SPEAK_MESSAGES` | Enable/disable speech playback in tool message code | Not found | `SOURCES/MESSAGE.CPP` |
| `OPT_QUICK_MESSAGES` | Skip/accelerate message pacing behavior in tool mode | Not found | `SOURCES/MESSAGE.CPP` |
| `OPT_CODES_JEU` | Mentioned in comment as behavior-affecting debug flag | Not found | `SOURCES/OBJECT.CPP` |

Related note:
- `SOURCES/3DEXT/LBA_EXT.H` defines `INFO_*` constants, but these are cube info field indexes (`INFO_ALPHA_LIGHT`, `INFO_NB_DECORS`, etc.), not `FlagInfos` bitmasks.

## Path and environment clues from historical tooling

Some tool-only constants still reference original Adeline-era local paths, for example:
- `PATH_IMPACT "f:\\projet\\lba2\\datas\\impact\\"`
- `PATH_FLOW "f:\\projet\\lba2\\datas\\flow\\"`

This supports the interpretation that current code preserves original in-house tool hooks rather than a fully modernized editor stack.
