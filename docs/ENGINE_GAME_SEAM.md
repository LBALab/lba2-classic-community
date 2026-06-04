# Engine / game / platform seam

> **Status: DRAFT / living document (v0).** Companion to [ARCHITECTURE_GLOBALS.md](ARCHITECTURE_GLOBALS.md).
> That doc cuts the LBA2 globals into eight *domains* (a within-game decomposition). This
> doc cuts along the deeper, orthogonal axis — **engine vs game vs platform** — using the
> sibling LBA1 source as an empirical oracle. Goal: make visible which code is reusable
> across titles, so strangler-fig boundaries can be drawn where they pay off twice.

## Why this axis, and why the module level

The engine/game line matters because of a real prospect: the `lba1-classic` community source
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
  `IMPACT`, `MESSTWIN`, and the holomap (`HOLO`, `HOLOBODY`, `HOLOGLOB`,
  `HOLOPLAN` — LBA1's single `HOLOMAP` re-grown). (`HERCULE` was grouped here in v0 but the
  per-module pin reclassifies it as **platform / dormant** — the Hercules debug display, not
  game content.)

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

## Per-module label table (pinned, v1)

The layer narrative above is grouped and v0; this section *pins every module* with a concrete
label, resolving the caveat "pin per-module before relying on a tag." It uses **two axes**,
because they are genuinely orthogonal:

- **Layer** — `engine` (reusable Adeline substrate), `game` (LBA2-specific logic/content),
  `platform` (host/OS/port abstraction). `engine·platform` marks a module that is split
  (an engine mechanism with a platform-specific tail).
- **Status** — `live` (built into the shipping binary and reached) or **`dormant`** (present
  in the tree but **not built or not called**). The directory layout does not encode this —
  a reusable *engine* codec (`DEC_XCF`) can be dormant, while a *platform* shim is live. The
  cross-title work surfaced the dormant axis: code one game exercises and another doesn't is
  engine **heritage**, not cruft (see the SDK timeline note below).

> Verification basis: `SOURCES/CMakeLists.txt` built sets (`SOURCES_FILES`, `…_ASM`,
> `…_WIN/_MACOS` are linked; `…_DOS`, `…_ASM_DOS` are **not**), plus a no-caller / not-included
> check for each module. ASM↔CPP rule below.

**ASM vs CPP.** Every `LIB386/**/*.ASM` and `SOURCES/*.ASM` is the original Adeline assembly,
kept as `reference`; the live code is the same-named `.CPP` port. Exceptions where *both* are
dormant: `COPY`, `DEC`, `DEC_XCF`, `HERCUL_A`, `KEYB`.

### LIB386 — the engine kernel + platform backends

| Module | Layer | Status | Role |
|---|---|---|---|
| `3D` | engine | live | Projection, rotation, matrices, camera |
| `ANIM` | engine | live | Animation frames + interpolation |
| `OBJECT` | engine | live | 3D object display (`AFF_OBJ`) |
| `pol_work` | engine | live | Polygon fillers (flat/gouraud/textured/zbuf/fog) |
| `SVGA` | engine·platform | live | Rasteriser (engine) + mode/screen setup (platform) |
| `SYSTEM` | engine·platform | live | HQR + file format (engine); keyboard/mouse/timer/IO (platform) |
| `MENU` | engine | live | Menu primitives |
| `AIL/` (interface) | engine | live | Audio abstraction (samples/music/mixing) |
| `AIL/SDL` | platform | live | Active audio backend (SDL3) |
| `AIL/NULL` | platform | live | Null audio backend (stub) |
| `AIL/MILES` | platform | dormant | DOS Miles Sound System backend |
| `FILEIO` | platform | live | File IO + `SAVEPNG` (port infra) |
| `SMACKER` / `libsmacker` | platform | live | Vendored Smacker decoder (LGPL) — LBA2 cinematics |
| `VIDEO_AUDIO_RESAMPLE` | platform | live | Audio resample for video (port infra) |
| `SNAPSHOT` | platform | live | Screenshot / test-harness capture |
| `H` | — | — | Shared headers / type + struct contracts |

### SOURCES — engine framework core (present in all three trees)

| Module(s) | Layer | Status | Role |
|---|---|---|---|
| `GERELIFE`, `GERETRAK`, `FUNC`, `EXTFUNC` | engine | live | Life/Track **script VM** + opcode tables — the membrane |
| `OBJECT`, `PERSO`, `EXTRA`, `FICHE` | engine | live | Entity / main-loop / projectile / object-data framework |
| `GRILLE` | engine | live | Brick-isometric world / scene render |
| `DISKFUNC` | engine | live | HQR resource loading |
| `MESSAGE`, `INTEXT` | engine | live | Dialog / speech / in-game text presentation |
| `GAMEMENU` | engine | live | Menu system |
| `AMBIANCE` | engine | live | Ambient-audio scheduler |
| `INCRUST`, `FIRE`, `FLOW`, `PLASMA`, `RAIN` | engine | live | Overlay / particle / effect rendering |
| `PATCH` | engine | live | Runtime patch of compiled script/scene pointers |
| `MEM` | engine | live | Memory management |
| `LZSS`, `COMPRESS` | engine | live | Compression (LZSS / LZMIT) |
| `MUSIC` | engine·platform | live | Music playback (engine logic + backend) |
| `COMMON`, `C_EXTERN`, `DEFINES`, `GLOBAL`, `VERSION` | engine | live | Types, globals, version scaffolding (the shared bus) |

### SOURCES — engine evolution (LBA2-added, engine-grade)

| Module(s) | Layer | Status | Role |
|---|---|---|---|
| `SORT` | engine | live | Sort tree (LBA1 used `BUBSORT`) |
| `POF`, `ZV`, `BEZIER` | engine | live | 3D model format / bounding volumes / curves |
| `ANIMTEX` | engine | live | Animated textures |
| `SCAN` | engine·platform | live | Key/bit-scan input helpers |
| `VALIDPOS` | engine | live | Collision / position validation |
| `FLOW_A`, `GRILLE_A` | engine | live | C ports of former flow / grille ASM |
| `3DEXT/` (`TERRAIN`, `DRAWSKY`, `DECORS`, `LOADISLE`, `BOXZBUF`, `LINERAIN`, `MAPTOOLS`, `GLOBEXT`, `LBA_EXT`, `VAR_EXT`) | engine | live | Exterior 3D: terrain, sky, decor, island load, z-buffer |
| `INITADEL` | engine | live | Engine init |
| `PLAYACF` | engine·platform | live | Video playback — **now Smacker** (its XCF codec `DEC_XCF` is dormant) |
| `SAVEGAME` | engine·game | live | Save system (engine mechanism + LBA2 save layout) |
| `CONFIG` | engine·platform | live | Config load/store |

### SOURCES — LBA2 game content

| Module(s) | Layer | Status | Role |
|---|---|---|---|
| `INVENT` | game | live | Inventory |
| `BUGGY`, `WAGON`, `DART` | game | live | Vehicles / minigames |
| `COMPORTE` | game | live | Behaviours |
| `CHEATCOD` | game | live | Cheat codes |
| `IMPACT` | game | live | Impact scripts |
| `HOLOBODY`, `HOLOGLOB`, `HOLOPLAN` | game | live | Holomap (LBA1's single `HOLOMAP`, re-grown) |
| `MESSTWIN` | game | live | Twinsen chapter-message data tables |
| `CREDITS`, `CREDITS_PARSE` | game | live | End credits |

### SOURCES — platform / port layer (this fork)

| Module(s) | Layer | Status | Role |
|---|---|---|---|
| `CONTROL`, `PERFTRACE`, `DEMO_SEED` | platform | live | Test harness / determinism / perf trace |
| `RES_DISCOVERY`, `RES_MENU`, `RES_PICKER`, `RES_SWITCH`, `DIRECTORIES`, `HQR_NAMES` | platform | live | Resource discovery / data-dir routing |
| `INPUT`, `JOYSTICK`, `TOUCH_INPUT` | platform | live | Input (SDL3 keyboard / gamepad / touch) |
| `IMAGE_LOAD` | platform | live | Image (PCX/etc.) loading |
| `ASSET_PREFLIGHT` | platform | live | Asset validation at boot |
| `CONSOLE/` (`CONSOLE`, `_CMD`, `_GIVE`, `_STATE`) | platform | live | Debug console (`CONSOLE_MODULE`) |
| `GAMEMENU_MOUSE`, `AUDIO_BALANCE`, `EXIT_SCREEN`, `FOLLOWCAM_CFG` | platform | live | QoL / port features |
| `SAVEGAME_LOAD_BOUNDS`, `SAVENAME_VALIDATION` | platform | live | Save-path hardening |
| `BUILD_INFO`, `EMBEDDED_CFG_WRITE` | platform | live | Build metadata / embedded default config |

### Dormant register (present in tree, not in the shipping binary)

The actionable cross-cut. **Do not delete without recording the heritage** — most of this is
the Adeline engine's *other-era* surface, the evidence that LIB386 is a multi-game SDK.

| Module | Would-be layer | Why dormant | Heritage |
|---|---|---|---|
| `DEC`, `DEC_XCF` (+`.ASM`) | engine | not built / no callers | The **XCF cinematic codec** — superseded by Smacker (`PLAYACF`→libsmacker). Decodes Time Commando `.ACF`; see [TIME_COMMANDO learning notes](TIME_COMMANDO.md). |
| `HERCULE`, `HERCUL_A` (+`.ASM`) | platform | `…_DOS` set, not built | Hercules mono adapter (`0xB0000`) — second-monitor debug display |
| `KEYB` (+`.ASM`) | platform | `…_ASM_DOS`, not built | DOS keyboard handler — replaced by `INPUT`/SDL3 |
| `CRITICAL` | platform | `…_DOS` set, not built | DOS critical-error handler |
| `COPY` (+`.ASM`) | platform | not built, source `/*`-wrapped | Frame-copy primitives — library is already pitch-aware |
| `CONFIG/` (`MAIN`, `AFFKEY`, …) | platform | separate tool, no CMake target | Original Adeline standalone config utility |
| `AIL/MILES` | platform | backend not selected | DOS Miles Sound System audio backend |
| all `*.ASM` originals | (matches `.CPP`) | reference, not assembled | Original Adeline x86 — the `.CPP` port of the same name is live |

### Version / dual-path seams (the SDK timeline)

Several *live* modules carry **two code paths for one job** because the Adeline formats evolved
across titles (verified against LBA1 1994 / Time Commando 1996 / LBA2 1997 retail data):

| Module | Dual path | Transition it bridges |
|---|---|---|
| `LIB386/SYSTEM` (LZ) | LZSS **+** LZMIT | LZMIT added 1994→96 (TC already has it) |
| `LIB386/AIL` samples | VOC **+** RIFF/WAV | VOC (LBA1) → WAV (TC, LBA2) |
| `PLAYACF` ↔ `DEC_XCF` | Smacker (live) vs XCF (dormant) | cinematics XCF (TC) → Smacker (LBA2) |
| `POF` / body loader | 16-bit (LBA1/TC) vs 32-bit (LBA2) coords | geometry widened 1996→97 |

These seams are the SDK's backward-compatibility made physical. When labelling, a dual-path
module is `engine` whose *liveness spans a format version boundary* — useful to flag before a
facade assumes a single format.

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
- The domain-level tags here are coarse; the [per-module label table](#per-module-label-table-pinned-v1)
  pins each module (layer + live/dormant) and is the authority for a specific facade. It also
  adds the **dormant** axis the layer narrative omits.

## Related docs — the three axes

This map is one of three orthogonal views of the same engine; use them together:

- **[ARCHITECTURE_GLOBALS.md](ARCHITECTURE_GLOBALS.md)** — *structure*: which domain owns
  which global (the shared-bus offenders).
- **This doc** — *layer*: engine vs game vs platform (what is reusable across titles).
- **[LIFECYCLES.md](LIFECYCLES.md)** — *time*: when state is created, mutated, destroyed
  (main-loop frame order, the `ChangeCube` phases, object/anim/behaviour state machines).

And the *data contract* underneath all three:

- **[ENGINE_FILE_FORMATS.md](ENGINE_FILE_FORMATS.md)** — the on-disk formats the engine reads
  (HQR, LZ, body, anim, sprite, samples, XCF, …), at engine altitude, with the cross-title
  version timeline. The dormant/dual-path seams in the per-module table are the codecs that
  doc specs.

The temporal axis sharpens this one twice over:

- The main-loop frame order shows the engine/game membrane *in motion* — track scripts at
  step 3, life scripts at step 6, every frame, per object.
- `ChangeCube`'s six phases are a tour through the domains in dependency order (grid →
  objects → zones → `CubeMode` → camera). That sequence is the spec a World/cube facade
  must preserve: it tells you not just *what* the boundary owns but the *order* in which a
  cube transition crosses it.
