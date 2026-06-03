# Engine / game / platform seam

> **Status: DRAFT / living document (v0).** Companion to [ARCHITECTURE_GLOBALS.md](ARCHITECTURE_GLOBALS.md).
> That doc cuts the LBA2 globals into eight *domains* (a within-game decomposition). This
> doc cuts along the deeper, orthogonal axis — **engine vs game vs platform** — using the
> sibling LBA1 source as an empirical oracle. Goal: make visible which code is reusable
> across titles, so strangler-fig boundaries can be drawn where they pay off twice.

## Why this axis, and why the module level

The engine/game line matters because of a real prospect: `~/code/lba-hacking/lba1-classic`
is the **LBA1 original Adeline source** (Relentless, 1994) — same lineage as this repo,
same `LIB386/`+`SOURCES/` layout, same HQR format. One day LBA1 content could run on this
engine. Even before then, LBA1 is useful *now*: it tells us empirically where the
engine/game line runs.

The naive way — grep LBA1 for each of the ~200 LBA2 globals — fails: LBA1 is C with
different identifiers, so per-global name matching is mostly misses. The robust unit is
the **module**. Modules survive renaming (LBA1 `BUBSORT` → LBA2 `SORT`; LBA1 `HOLOMAP` →
LBA2 `HOLO*`), and a three-way module diff across the trees separates the layers cleanly.

### The three trees

| Tree | Game | Era | Language |
|---|---|---|---|
| `lba1-classic` | LBA1 (Relentless) | 1994 | C + ASM |
| `lba2-classic` | LBA2 (original Adeline) | 1997 | C++ + ASM |
| `lba2-classic-community` | LBA2 (this fork) | 1997 + port | C++98 + ASM, SDL3 |

Reproduce the diff with `comm` over the ext-insensitive module stems of each tree's
`SOURCES/` (and compare `LIB386/` submodule names).

## LIB386 kernel evolution

The engine kernel is recognizable across all three, renamed and consolidated:

| LBA1 | LBA2 / community | Role |
|---|---|---|
| `LIB_3D` | `3D` | Projection, rotation, matrices |
| `LIB_SVGA` | `SVGA` | Rasteriser / framebuffer |
| `LIB_SYS` | `SYSTEM` | HQR, files, keyboard, mouse, timer |
| `LIB_MENU` | `MENU` | Menu primitives |
| `LIB_MIX` / `LIB_SAMP` / `LIB_MIDI` | `ail` / `AIL` | Audio (mixing, samples, music) |
| `LIB_CD` | (folded into system/audio) | CD audio |
| — | `ANIM`, `OBJECT`, `pol_work` | LBA2 engine growth (anim interp, object display, polygon fillers) |
| — | `SNAPSHOT`, `VIDEO_AUDIO_RESAMPLE`, `libsmacker` | Community port infrastructure |

The kernel evolved (more 3D, polygon fillers split out, audio unified behind `ail`) but the
spine — projection, raster, system/HQR, sound — is continuous.

## SOURCES module layers (the three-way diff)

### Layer 1 — durable engine / framework core (in all three)

`AMBIANCE` `COMMON` `C_EXTERN` `DEFINES` `DISKFUNC` `EXTRA` `FICHE` `FIRE` `FUNC`
`GAMEMENU` `GERELIFE` `GERETRAK` `GLOBAL` `GRILLE` `INCRUST` `MESSAGE` `OBJECT` `PERSO`
`VERSION`

These 19 modules survived 1994 → 1997 → 2024. They are the engine-game framework:

| Module(s) | Role | Engine mechanism vs game content |
|---|---|---|
| `GERELIFE`, `GERETRAK`, `FUNC` | Life/Track **script VM** and its opcode functions | **The membrane.** Engine interprets; the scripts are game. |
| `OBJECT`, `PERSO`, `EXTRA`, `FICHE` | Entity / character / projectile / object-data framework | Engine framework; the behaviours and stats are game content. |
| `GRILLE` | Brick-isometric world / scene render | Engine; the specific scenes are content. |
| `DISKFUNC` | Resource loading (HQR) | Engine. |
| `MESSAGE` | Dialog / speech presentation | Engine machinery; the text is content. |
| `GAMEMENU` | Menu system | Engine machinery; the menu set is content. |
| `AMBIANCE` | Ambient-audio scheduler | Engine mechanism; the per-cube sample tables are content. |
| `INCRUST`, `FIRE` | Overlays / fire effect | Engine rendering effects. |
| `COMMON`, `C_EXTERN`, `DEFINES`, `GLOBAL`, `VERSION` | Types, globals, version scaffolding | Framework scaffolding (the global bus lives here). |

### Layer 2 — LBA2 additions (engine evolution + LBA2 game)

In `lba2-classic` but not `lba1-classic`. Two sub-kinds:

- **Engine evolution:** `SORT` (the sort tree — LBA1 used `BUBSORT`), `POF` (3D model
  format), `ZV` (bounding volumes), `BEZIER`, `ANIMTEX`, `DEC`/`DEC_XCF` (decor),
  `EXTFUNC`/`PTRFUNC` (script function tables), `INPUT`/`JOYSTICK`/`KEYB` (input split
  out), `MEM`, `SAVEGAME`, `CONFIG`, `FLOW`, `PLAYACF` (video), `SCAN`, `VALIDPOS`.
- **LBA2 game content:** `BUGGY`, `WAGON`, `DART`, `INVENT`, `CHEATCOD`, `COMPORTE`,
  `HERCULE`, `IMPACT`, `MESSTWIN`, and the holomap (`HOLO`, `HOLOBODY`, `HOLOGLOB`,
  `HOLOPLAN` — LBA1's single `HOLOMAP` re-grown).

### Layer 3 — community / port additions (in this fork only)

`ASSET_PREFLIGHT` `AUDIO_BALANCE` `BUILD_INFO` `COMPRESS` `CONTROL` `COPY` `CREDITS_PARSE`
`DEMO_SEED` `DIRECTORIES` `EMBEDDED_CFG_WRITE` `EXIT_SCREEN` `FLOW_A` `FOLLOWCAM_CFG`
`GAMEMENU_MOUSE` `GRILLE_A` `HERCUL_A` `HQR_NAMES` `IMAGE_LOAD` `INITADEL` `PERFTRACE`
`PLASMA` `RES_DISCOVERY` `RES_MENU` `RES_PICKER` `RES_SWITCH` `SAVEGAME_LOAD_BOUNDS`
`SAVENAME_VALIDATION` `TOUCH_INPUT`

Almost entirely **platform / port**: resource discovery (`RES_*`, `DIRECTORIES`,
`HQR_NAMES`), de-ASM'd primitives (`COPY`, `COMPRESS`, `*_A` C ports of old ASM), the test
harness (`CONTROL`, `PERFTRACE`), input/IO (`TOUCH_INPUT`, `IMAGE_LOAD`), with a few QoL
features (`GAMEMENU_MOUSE`, `FOLLOWCAM_CFG`, `AUDIO_BALANCE`, `PLASMA`). The fork has not
touched the engine core or the game — it has been building the platform layer underneath.

### LBA1-only (modernized away by LBA2)

`ADFLI_A` `BALANCE` `BUBSORT` `CPYMASK` `FLA` `FLIPBOX` `HOLOMAP` `MCGA` `MIXER` `PLAYFLA` —
old platform tech (`MCGA` display, `FLA`/`PLAYFLA` video, `MIXER`/`BALANCE` audio,
`BUBSORT`). Confirms the churn between titles was mostly *platform*, not game logic.

## Layer tags on the eight domains

Mapping [ARCHITECTURE_GLOBALS.md](ARCHITECTURE_GLOBALS.md)'s domains onto this axis, with
the owning module(s) as evidence. Almost every domain is **engine mechanism + game
content** — the split is rarely a whole domain, it runs *through* each one:

| Domain | Layer | Evidence / note |
|---|---|---|
| Gameplay / simulation | **Game** on an engine substrate | `OBJECT`/`PERSO`/`EXTRA` framework is engine (all three); behaviours, inventory, magic, stats are LBA2 content driven by the script VM. |
| Rendering | **Engine** | `3D`/`SVGA`/`GRILLE`/`INCRUST` in all three; `SORT`/`pol_work` are LBA2 engine evolution. |
| World / cube transform | **Engine** | `GRILLE` + `DISKFUNC` world model in all three; specific cube/island data is content. |
| Resources / assets | **Engine** | HQR (`LIB_SYS`→`SYSTEM`) + `DISKFUNC` in all three; the archives are content. |
| Audio | **Engine** mechanism + game scheduling | `AMBIANCE` in all three; mixing via `ail`; per-cube sample tables are content. |
| Platform / IO | **Platform** | `SYSTEM`/`SVGA` + the community port layer (`CONTROL`, `RES_*`, `TOUCH_INPUT`). This is the PAL. |
| UI / text | **Engine** machinery + game content | `MESSAGE`/`GAMEMENU` in all three; the menus, text, holomap are content. |
| Orchestration / lifecycle | **Engine** framework | Main loop / scene lifecycle / `INITADEL`; the specific game flow is content. |

## What this means for prioritisation

- **The script VM (`GERELIFE`/`GERETRAK`/`FUNC`) is the keystone seam.** It is the literal
  engine/game interface, present in all three trees. The dynamic proof is in
  [LIFECYCLES.md](LIFECYCLES.md): the per-frame main loop crosses the membrane at **step 3
  (`DoTrack`, track scripts)** and **step 6 (`DoLife`, life scripts)** for every object —
  the engine drives the loop, the game's scripts are invoked. A clean boundary here is what
  makes the engine title-agnostic — the highest-leverage facade, aligned with the
  Life/Track tooling thread (#181). Full API spec:
  [ENGINE_GAME_INTERFACE.md](ENGINE_GAME_INTERFACE.md).
- **Engine-seam facades pay off twice.** Drawing a facade around an all-three module
  (World/cube via `GRILLE`+coords, Resources via HQR/`DISKFUNC`) cleans up LBA2 now and is
  reusable for LBA1 later. Prefer these over facades around Layer-2 game content
  (`INVENT`, magic ball), which differ per title by definition.
- **The platform seam is already on this path.** The community layer (Layer 3) is the PAL;
  PR #284–#286 formalise it. Continuing to push platform concerns into that layer keeps
  the engine core clean for both titles.

## Caveats

- **Renaming and language.** LBA1 is C; modules and identifiers were renamed and
  reorganised for LBA2 (`BUBSORT`→`SORT`, `HOLOMAP`→`HOLO*`, `LIB_3D`→`3D`). Module
  presence is evidence of a *shared concept*, not drop-in compatibility.
- **Format and capability divergence.** LBA1 used older formats (FLA video, MCGA) and a
  more limited 3D path. Hosting LBA1 here means bringing its *content* onto this *newer*
  engine, not merging two equals.
- **twin-e / TwinEngine** already runs both titles via reimplementation — feasibility is
  proven. The original-source path is harder but preservation-faithful. The concrete
  per-subsystem porting cost is assessed in [LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md).
- Layer tags here are v0 judgments from module evidence; pin per-module before relying on a
  tag for a specific facade.

## Related docs — the three axes

This map is one of three orthogonal views of the same engine; use them together:

- **[ARCHITECTURE_GLOBALS.md](ARCHITECTURE_GLOBALS.md)** — *structure*: which domain owns
  which global (the shared-bus offenders).
- **This doc** — *layer*: engine vs game vs platform (what is reusable across titles).
- **[LIFECYCLES.md](LIFECYCLES.md)** — *time*: when state is created, mutated, destroyed
  (main-loop frame order, the `ChangeCube` phases, object/anim/behaviour state machines).

The temporal axis sharpens this one twice over:

- The main-loop frame order shows the engine/game membrane *in motion* — track scripts at
  step 3, life scripts at step 6, every frame, per object.
- `ChangeCube`'s six phases are a tour through the domains in dependency order (grid →
  objects → zones → `CubeMode` → camera). That sequence is the spec a World/cube facade
  must preserve: it tells you not just *what* the boundary owns but the *order* in which a
  cube transition crosses it.
