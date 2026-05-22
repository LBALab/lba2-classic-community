# Initialisation research

How the engine gets from process start to a running scene, for both **new game** and **load
save**. Covers the gates, the timing/speed model, what each step does, and cleanup candidates.

Everything below is traced from the current tree. File:line references are clickable. The init
path is almost entirely original Adeline structure — the port has added seams (CLI harness,
fixed-dt clock, embedded config) but not reordered the boot.

## The shape of it

There are two boot trajectories that converge on the same per-game init:

```
main()                                  PERSO.CPP:2275
 ├─ one-time engine + resource bring-up  (always, ~everything below "Phase A/B")
 └─ dispatch:
      ├─ SaveLoadTestMode   → InitGame(-1) → ChangeCube()         (issue #62 harness)
      ├─ Control_IsActive() → Control_Begin() → MainLoop()        (CLI control harness)
      └─ MainGameMenu(load) → user picks New / Load / Continue     (normal play)
                               └─ InitGame(±1) → [ChangeCube()] → MainLoop()
```

`InitGame()` sets up game-global state; `ChangeCube()` (directly, or via `MainLoop`'s first
iteration) loads the actual scene. **New game** and **load save** differ only in the argument to
`InitGame` and a handful of `FlagLoadGame` branches inside `ChangeCube`.

## Phase A — process bootstrap (`main` → `InitAdeline`)

`main()` (`PERSO.CPP:2275`) runs a fixed prologue before any game logic:

| Step | Call | What / why |
|------|------|------------|
| `--version` short-circuit | inline | Prints `LBA2_VERSION_STRING` and exits before SDL — works headless/CI. |
| Flag parsing | `parse_save_load_test_flag`, `Control_ParseArgs` | Strips harness flags out of `argv` so the rest of `main` sees a clean arg list. |
| SDL core | `SDL_Init(0)` | Needed *before* filesystem APIs. Note: deliberately no `SDL_Quit()` on the early exit paths — SDL3 can double-free in `SDL_QuitFilesystem`. |
| Path resolution | `GetDefaultUserDir`, `ResolveGameDataDir`, `PromptForResDir`, `InitDirectories` | Finds retail data (`--game-dir`, env, `./data`, `../LBA2`); last resort is a folder-picker dialog, persisted for next launch. Headless → exit. |
| Big buffer | `InitMainBuffer()` | See "memory model" below. Allocates the single arena for all fixed engine buffers. |
| `InitAdeline(argc, argv)` | `INITADEL.C:60` | Brings up every OS-facing subsystem. |

`InitAdeline` (`INITADEL.C:60`) is the subsystem bring-up, in this strict order — each failure
`exit(1)`s with a TODO for "graceful exit":

1. **Logging** — resolves asset/save/config/log paths, opens the log (`CreateLog`).
2. **Events** (`InitEvents`) → **Joystick** (`InitJoystick`) → **Window** (`InitWindow`).
3. **Config file existence** — if `lba2.cfg` is missing, copy the default from assets, else write
   an embedded template (`WriteEmbeddedDefaultLba2Cfg`). This is *creation only*; values are read
   later in `ReadConfigFile`.
4. **Command line** (`GetCmdLine`), **OS id** (`DisplayOS`).
5. **CPU** — `ProcessorIdentification()` is commented out; the signature is **hardcoded** to a
   Pentium-class FPU, MMX off (`INITADEL.C:143-148`). Legacy ASM dispatch relic.
6. **AIL** (`InitAIL`) — audio API, also provides `vmm_lock`/timer. Comment flags it as
   mis-located ("reposition closer to sound subsystem").
7. **Video** — `InitVideo` → `InitScreen` → `InitGraphics(RESOLUTION_X, RESOLUTION_Y)`.
8. **Midi** (compile-gated), **Sample driver** (`InitSampleDriver`).
9. **Smacker** — commented out (libsmacker is initialised elsewhere/lazily).
10. **Keyboard** → **Mouse** → **Timer** (`InitTimer`) → **DefFile buffer** (parses the cfg file
    into `ibuffer`, which is `ScreenAux` reused as scratch).

Back in `main`, post-`InitAdeline`:
- Console hooks installed (`SetEventFilter`, `SetPrePresentCallback`) — port addition.
- `atexit(TheEndInfo)` registers the shutdown report (`PERSO.CPP:2133`).
- `InitProgram()` (`PERSO.CPP:2064`): `BoxInit`, clear screen, `MemoClipWindow`, optional
  `InitVoiceFile` (CDROM), then **`ReadConfigFile()`** — this is where speed/detail settings land.
- `InitMemory()` (`MEM.CPP:193`): the small per-feature buffers (textures, scene, particles, …).

## Phase B — resource banks (`main`, the long middle)

This is the bulk of wall-clock boot time: opening and indexing HQR resource banks. Each is a
`HQR_Init_Ressource` (lazy, indexed) or `LoadMalloc_HQR`/`Load_HQR` (eager) against a path built by
`GetResPath`. Failures route to `TheEndCheckFile` (distinguishes missing-file from out-of-memory).

Order (all in `main`, ~`PERSO.CPP:2361-2626`):

- **Samples** bank (if `Sample_Driver_Enabled`), then locked.
- `InitLanguage()` (`MESSAGE.CPP:308`) — multilanguage selection.
- `BufText`/`BufOrder` dialogue buffers; `InitDial(0)`; pull "please wait" / "new game" strings.
- **Palettes**: `InitPalette` + `ChoicePalette` — done early because `InitDial(2)`/`AskForCD` need them.
- **CDROM block** (compile-gated): loads screen HQR, scans for the disc, loops on `AskForCD`. In
  DEBUG/TEST builds it's just `InitCD("")`. Dead on normal desktop builds.
- `InitJingle()`, animation buffer (`BufferAnim`), font (`LbaFont` → `SetFont`/`ColorFont`),
  `InitAcf()` (cutscene player).
- **Logos** (non-DEBUG, non-harness): `DistribLogo`, `AdelineLogo` (skipped after first launch via
  `IsFirstGameLaunched`), `DemoBumper` (DEMO).
- `InitDial(2)` (ensures voice file on disk), `Init3DExt()` (terrain/sky/rain subsystem).
- **3D file table** (`RESS_FILE3D`) — *must* precede `LoadFicPerso()`.
- `LoadFicPerso()` (`OBJECT.CPP:559`) — loads every behaviour's hero 3D body.
- `InitObjects(BufferAnim, …)` (`LIB386/ANIM/LIBINIT.CPP:22`) — animation system arena + body/anim
  pointer-resolver callbacks.
- Texture/goodies/anim banks: `BufferTexture` (+ `ScanTextureAnimation`/`InitTextureAnimation`),
  `PtrZvExtra`, `PtrZvExtraRaw`, `PtrZvAnim3DS`, sprite banks, 3DS anim bank (+`LoadListAnim3DS`),
  POF, impacts, **anims**, **bodies**, **objfix**.
- Particle/effect inits: `InitPartFlow`, `InitDarts`, `InitRain`, `InitSysText`.
- `PtrPolySea` — animated-sea buffer, *must be the last allocation*; if it fails, sea animation is
  silently disabled (`MaxPolySea = 0`).

After this point the engine is fully resourced; only scene-specific data is loaded later.

## Phase C — boot dispatch

`main` then chooses a trajectory (`PERSO.CPP:2655-2769`):

- **DEBUG cube-select** (`./lba2 <n>`): `InitGame(1)`, set `NewCube`, `MainLoop()`.
- **Save on command line** (`ArgV[1]`): resolve to `GamePathname` (direct → save dir → `.lba`
  suffix), set `load = TRUE`.
- **`SaveLoadTestMode`** (`--save-load-test`): `InitGame(-1)` → `ChangeCube()`, print parseable
  stage lines, exit. Pure load-path probe.
- **`Control_IsActive()`** (CLI control harness): `Control_Begin()` then `MainLoop()`.
  `Control_Begin` (`CONTROL.CPP:166`) pins the deterministic clock *first*
  (`Timer_EnableFixedDt`), then either resolves+loads a save (`InitGame(-1)`→`ChangeCube()` while
  `FlagLoadGame` is set) or starts fresh (`InitGame(1)`). Comment stresses the ordering: ChangeCube
  must run before `MainLoop` clears `FlagLoadGame`.
- **Otherwise** `MainGameMenu(load)` — normal play.

## Phase D — per-game init (`InitGame`)

`InitGame(argc)` (`PERSO.CPP:292`) resets game-global state. **`argc` is overloaded as a mode flag,
not a count:**

- `argc == 1` → new game (fresh state, fade to black, return to caller).
- `argc == -1` → load (sets `FlagLoadGame = TRUE`, calls `LoadGameNumCube()` to read the target
  cube + game-vars header out of the save).

Common to both:
- `UnsetClip()`, light angles (`AlphaLight`/`BetaLight`), `PtrInit3DGame()` (selects
  interior/exterior projection + function pointers — `INTEXT.CPP:54`).
- `InitGameLists()` (`PERSO.CPP:259`) — clears incrust list, `ListVarCube`/`ListVarGame`, buggy,
  ardoise; `InitInventory()`; resets `NbObjets`/`NbZones`/`NbBrickTrack`.
- `InitPerso()` (`OBJECT.CPP:580`) — Twinsen object defaults (life, body, weapon, counters).
- `InitHoloMap()`.
- Scene-start coordinates and a long block of game-global scalars (magic, money, keys, chapter
  flag, weapon, island, clovers, behaviour = `C_NORMAL`, `CinemaMode = FALSE`).
- `NewCube = 0; NumCube = -1;` — the first `MainLoop` iteration will load cube 0 (new game) or the
  cube from the save header (load).

The **new-game cinematic** (`Introduction()`, `GAMEMENU.CPP:4592`) runs from the menu's `new_game`
label, *after* `InitGame(1)` — not inside `InitGame`.

## Phase E — scene/cube load (`ChangeCube` → `LoadScene`)

`ChangeCube()` (`OBJECT.CPP:1157`) is the per-scene loader, invoked from `MainLoop` whenever
`NewCube != -1`. This is the heaviest recurring init and the shared tail of both boot paths.

Sequence:
1. `srand(TimerRefHR)` — **RNG is reseeded from the clock on every cube change.** Under fixed-dt
   this is deterministic; under wall-clock it is the main nondeterminism source after dt.
2. Optionally memorise current anim/frame for seamless resume (`FlagReinit` from `FlagChgCube`).
3. `ClearScene()` (`OBJECT.CPP:987`) — extras, cube vars, ambiance, lightning, incrust, dialogue.
4. Hero soft-reset (move/zone/track fields).
5. `LoadScene(NewCube)` (`DISKFUNC.CPP:50`) — loads the `.SCC` blob from the scene HQR and
   *parses it inline* via `GET_S8/S16/U32` macros: island/planet, cube coords, shadow level,
   labyrinth flag, **CubeMode (interior/exterior)**, ambiance (4 sample slots, light angles,
   jingle), hero start, then the object table (`InitObject` + flags + 3D-file index per object;
   `LoadFile3D` only when *not* loading a save), zones, tracks. The mid-parse `FadeToBlackAndSamples`
   is skipped when loading a save or doing a seamless exterior→exterior transition.
6. `ManageSystem()` is sprinkled throughout (`ChangeCube` calls it ~6×, `LoadScene` once) — see
   timing section; it is the streaming/event pump.
7. `ClearHQRs()` when cube mode changed or end-game — frees HQR banks that are ≥2/3 full
   (`CleanHQR` macro, `OBJECT.CPP:1010`).
8. `InitDial(START_FILE_ISLAND + Island)`, `PtrInitGrille(NewCube)` (loads the brick grid — the
   interior/exterior variant chosen by `PtrInit3DGame`).
9. Hero placement: from `NewPos*`/`CubeStart*` (new/transition) — but **skipped entirely when
   `FlagLoadGame`** (the save holds the position). Position is re-snapped to valid ground via
   `PtrReajustPos` unless carried/gravity-bound.
10. `RestartPerso()` (`OBJECT.CPP:911`), then the load/new fork:
    - **New game** (`!FlagLoadGame`): `StartInitAllObjs()` (`OBJECT.CPP:510`) initialises every
      scene object via `StartInitObj`; `RestoreTimer()`; reapply protection spell.
    - **Load** (`FlagLoadGame`): `LoadGame()` (version-checked; `LoadGameOldVersion` for old
      saves in debug). Then `InitLoadedGame()` (`OBJECT.CPP:1033`) — re-resolves bodies/anims/
      sprites and `CarryBy` links for the deserialised objects — *unless* the context read failed
      (`LOADGAME_ERR_CONTEXT`, guarded so a partial `ListObjet` doesn't segfault), in which case it
      degrades. `LoadFile3dObjects()` runs on the success-with-positive-flag branch.
      `CheckProtoPack()` afterward.
11. Buggy mode restore, camera centring (`CameraCenter`), light vector, music start/stop, particle
    flow reset, currency selection by island, `NewCube = -1` (consumed).

### New game vs load — the actual differences

| Aspect | New game (`InitGame(1)`) | Load (`InitGame(-1)`) |
|--------|--------------------------|------------------------|
| `FlagLoadGame` | FALSE | TRUE |
| Target cube | 0 | from save header (`LoadGameNumCube`) |
| Hero position | from scene `CubeStart*`/`NewPos*` | from save (placement block skipped) |
| Object 3D files | loaded during `LoadScene` parse | loaded later in `InitLoadedGame`/`LoadFile3dObjects` |
| Object state | `StartInitAllObjs` (defaults) | `LoadGame`+`InitLoadedGame` (deserialised) |
| Timer | `RestoreTimer()` | `SetTimerHR(savetimerrefhr)` from save |
| Palette/music | from scene | `ChoicePalette` + `RestartMusic` from `LoadGame` |

## Timing and speed model — the real gates

The engine is **dt-based, not fixed-step**. There is no game-loop frame cap.

- **`ManageTime()`** (`TIMER.CPP:101`) is the clock. Each call reads `SDL_GetTicks()` (or the
  virtual clock under fixed-dt) and *accumulates the wall-clock delta into `TimerRefHR`* unless the
  timer is locked. Everything time-driven (anims, ambiance, falling, protopack timers, message
  durations) reads `TimerRefHR`. So **on a faster machine the loop runs more often but each step
  advances `TimerRefHR` by less — the simulation is wall-clock-paced.** This is exactly the
  variable-dt behaviour characterised for the determinism work.
- **`ManageSystem()`** (`TIMER.H:43`) = `ManageEvents()` + `ManageTime()`. It is called repeatedly
  *inside* `ChangeCube`/`LoadScene` so that the OS event queue is pumped and the clock advances
  during a long load ("for streaming"). It does not render.
- **Frame pacing** is effectively the present path: `SDL_SetRenderVSync(sdlRenderer, 1)` is set
  unconditionally in `WINDOW.CPP:159`. So the present is vsync-bound, but the *simulation* is not —
  it's whatever `TimerRefHR` delta accrued.
- **Timer save/restore stack**: `SaveTimer`/`RestoreTimer` use a counter (`CmptMemoTimerRef`) so the
  clock can be frozen across menus/loads and restored without leaking elapsed real time into the
  game. `LockTimer`/`UnlockTimer` (focus loss/gain) freeze accumulation while backgrounded.
- **Fixed-dt deterministic mode** (`Timer_EnableFixedDt`, `TIMER.CPP:78`): harness-only. Swaps the
  wall clock for a virtual clock advanced exactly once per tick by `Timer_FixedDtAdvance()`. Seeded
  so the first delta is 0 — no boot/load wall-clock leaks into the first tick. Default builds are
  byte-for-byte unaffected.
- **`DetailLevel`** (`ReadConfigFile` → `SetDetailLevel`, `GAMEMENU.CPP:538`): the user-facing
  speed/quality gate. 0 = no animated water/rain/horizon, minimal shadows; 3 = everything. Read
  from `lba2.cfg`, clamped to `[0, MAX_DETAIL_LEVEL]`. `SetDetailLevel` is called once at the top
  of `MainLoop`. This is the lever for "make it run on a slow machine."
- **FPS counter** (`NbFramePerSecond`): computed in `ManageTime` from `CmptFrame` (incremented at
  the end of the game frame, `PERSO.CPP:1911`); informational only, does not gate anything.

## Cleanup candidates

Observed while tracing — none are urgent, all are low-risk-to-note:

- **Overloaded `InitGame(int argc)`** (`PERSO.CPP:292`): the parameter is a tri-state mode flag
  (`1`/`-1`) misnamed `argc`. A named enum (`INIT_NEW`/`INIT_LOAD`) would remove a real
  readability trap — every call site passes a magic ±1.
- **Hardcoded CPU signature** (`INITADEL.C:143-148`): `ProcessorIdentification()` is commented out
  and the struct is stuffed with Pentium values. If no remaining code branches on it, delete the
  struct and the `/CPUNodetect` flag; if something does, that's a latent dead-feature.
- **`InitAIL` mislocation** — its own comment asks to move it next to the sound subsystem.
- **`exit(1)` TODOs** throughout `InitAdeline` ("Implement graceful exit") — currently a hard exit
  on any subsystem failure with no cleanup.
- **`V_ColorLine`** (`PERSO.CPP:2126`): a no-op stub that takes three args and discards them; only
  called by `TheEndInfo` for colour codes that no longer mean anything. The `// Trouver le moyen de
  cleaner cette @#&? de fonction` comment agrees.
- **`Message`** has a real DEBUG body and a no-op release body whose comment is an apology
  (`PERSO.CPP:2238`). Fine, but worth a one-liner instead of the franglais essay.
- **CDROM block in `main`** (`PERSO.CPP:2400-2455`): substantial dead weight on desktop builds —
  disc volume scan, `AskForCD` loop, `BufMemoSeek`. Compile-gated by `CDROM`; if no shipping target
  defines it, it's removable.
- **Commented Smacker init** (`INITADEL.C:198-208`) and **`GetDiskEnv`/`RestoreDiskEnv`**
  (`DISKFUNC.CPP:27-42`) — dead DOS-era blocks left as `/* */`.
- **`ArgC = ArgC;` warning-suppression idioms** (`PERSO.CPP:2635/2638`) — replaceable with
  `(void)ArgC;`.
- **`ManageSystem()` "for streaming" scatter**: ~7 calls across `ChangeCube`/`LoadScene` with
  identical comments. Whether they're all still load-bearing post-port (vs. DOS streaming relics)
  is worth a focused check — pumping events mid-load matters; the count may not.
- **French comments asking questions** ("Pourquoi ce test ????", "Faut-il le laisser ?") litter
  `ChangeCube`/`MainLoop`/`StartInitAllObjs` — each is a small archaeology task that could be
  resolved and removed.

## Verbatim TODO/FIXME inventory

Exact comment text as it appears in the tree, for the files on the init path. There are **no
literal `FIXME`/`XXX` tokens** here — markers are English `TODO`s (port-added) and legacy French
shorthand. French glossary: *à virer* = "to remove", *rustine* = "hack/patch", *crade* = "grubby",
*temporaire* = "temporary", *virer le warning* = "silence the warning".

### English `TODO`

| File:line | Verbatim comment | What it blocks |
|-----------|------------------|----------------|
| `INITADEL.C:85` | `exit(1); // TODO: Implement graceful exit` | Event init failure hard-exits, no cleanup. |
| `INITADEL.C:99` | `exit(1); // TODO: Implement graceful exit` | Window init failure. |
| `INITADEL.C:140` | `// TODO: Remove when all ASM is ported to C` | Guards the **hardcoded** `ProcessorSignature` block (`:143-148`). Detection stays stubbed *until ASM dispatch is gone* — ties to the "ASM is dead in shipping binary" finding; the remaining consumers were ASM paths. |
| `INITADEL.C:153` | `InitAIL(); // TODO: Reorganize/reposition closer to sound subsystem` | AIL is brought up here for `vmm_lock`/timer; comment wants it next to the sound subsystem. |
| `INITADEL.C:161` | `exit(1); // TODO: Implement graceful exit` | Video init failure. |
| `INITADEL.C:165` | `exit(1); // TODO: Implement graceful exit` | Screen init failure. |
| `INITADEL.C:169` | `exit(1); // TODO: Implement graceful exit` | Graphics init failure. |
| `INITADEL.C:192` | `exit(1); // TODO: Implement graceful exit` | Sample-driver init failure. |
| `GAMEMENU.CPP:1065` | `// TODO(gamepad): uses GetAscii() (keyboard-only) — pure-gamepad users cannot enter` | Save-name entry is keyboard-only; gamepad-only players can't type a save name. |
| `GAMEMENU.CPP:1846` | `} while (FileSize(GamePathname) != 0); // TODO: Review this sorcery` | Unexplained loop in the save-slot path; flagged as not understood. |

### Legacy French notes (cleanup intent, no English marker)

| File:line | Verbatim comment | Reading |
|-----------|------------------|---------|
| `PERSO.CPP:2124` | `// Trouver le moyen de cleaner cette @#&? de fonction !!!!!!` | "Find a way to clean this @#&? function" — over `V_ColorLine`, a no-op stub. |
| `PERSO.CPP:2240` | `// pour virer warnings !!!! Baaahhhh ! Que j'aime pas ça, c'est pas beau !!!` | Apology over the no-op release `Message` body. |
| `PERSO.CPP:2508` | `// a Faire Imperativement avant LoadFicPerso() !!!!!!!!` | "Must be done before `LoadFicPerso()`" — ordering constraint on the `RESS_FILE3D` load. |
| `PERSO.CPP:2634` | `// Pour virer warning !!!!!!` | Over `ArgC = ArgC;` — replaceable with `(void)ArgC;`. |
| `PERSO.CPP:2637` | `// temporaire pour virer Warning` | Same idiom in the DEBUG branch. |
| `DISKFUNC.CPP:208` | `// Message chapter (a virer)` | "(to remove)" — chapter-message debug print. |
| `OBJECT.CPP:543-544` | `// init zv pingouin en dur (je sais, c un peu crade, mais c 1 rustine` / `// de fin de projet !!!!!!)` | "hardcoded pingouin bounding volume (a bit grubby, end-of-project hack)" — in `RestartMecaPingouin`, called from `StartInitAllObjs`. |

Note: `MEM.CPP`, `TIMER.CPP`, `CONTROL.CPP`, `INTEXT.CPP`, and `SAVEGAME.CPP` carry no
TODO/FIXME/XXX markers on the init path.

## Quick call-tree reference

```
main                                              PERSO.CPP:2275
├─ InitMainBuffer                                 MEM.CPP:154   (one big arena)
├─ InitAdeline                                    INITADEL.C:60 (SDL/window/video/audio/input/timer)
├─ InitProgram → ReadConfigFile                   PERSO.CPP:2064 / 1934  (DetailLevel etc.)
├─ InitMemory                                     MEM.CPP:193   (per-feature buffers)
├─ [resource banks]                               PERSO.CPP:2361-2626 (HQR open/index, fonts, sprites…)
├─ Init3DExt / LoadFicPerso / InitObjects         EXTFUNC.CPP:62 / OBJECT.CPP:559 / LIBINIT.CPP:22
└─ dispatch
   ├─ MainGameMenu(load)                          GAMEMENU.CPP:3298
   │   ├─ new_game:  InitGame(1)  → Introduction → MainLoop
   │   └─ load_game: InitGame(-1) → ChangeCube    → MainLoop
   ├─ Control_Begin → MainLoop                    CONTROL.CPP:166
   └─ SaveLoadTestMode: InitGame(-1) → ChangeCube → exit

InitGame(±1)                                      PERSO.CPP:292
├─ PtrInit3DGame / InitGameLists / InitPerso / InitHoloMap
└─ argc==-1 ? LoadGameNumCube : FadeToBlack

MainLoop (per tick)                               PERSO.CPP:430
├─ SetDetailLevel (once)                          GAMEMENU.CPP:538
├─ if NewCube!=-1: ChangeCube                     OBJECT.CPP:1157
│   ├─ srand(TimerRefHR)
│   ├─ ClearScene / LoadScene(.SCC parse)         OBJECT.CPP:987 / DISKFUNC.CPP:50
│   ├─ PtrInitGrille (brick grid)
│   ├─ new : StartInitAllObjs                     OBJECT.CPP:510
│   └─ load: LoadGame → InitLoadedGame            SAVEGAME.CPP:783 / OBJECT.CPP:1033
├─ Control_TickHook                               CONTROL.CPP:382
├─ MyGetInput / ManageTime                        TIMER.CPP:101
└─ [gameplay + AffScene render]
```
