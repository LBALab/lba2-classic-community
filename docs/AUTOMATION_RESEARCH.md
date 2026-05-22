# Automation control surface — research

Phase 1 research for a CLI-driven automation harness (`--load`, `--exec`, `--tick`,
`--dump-state`, `--screenshot`, `--exit`). This is findings only — no design decisions,
no implementation. The plan lives in `AUTOMATION_PLAN.md` (Phase 2).

The headline: most of the moving parts already exist. There is a real console with a
string-execute entry point, a save→cube load sequence already driven headlessly by an
existing test harness, a working PNG screenshot path, and a `status` command that is
already a small watch window. The genuinely new work is an arg-parse hook, a per-tick
step driver, and a JSON state dump. The hard problem is determinism, and it is not new
to this proposal — the engine is wall-clock driven today.

Unless noted, line numbers are against the tree at the time of writing; treat them as
"look here," not exact.

---

## 1. Main loop structure

Entry: `main()` at `SOURCES/PERSO.CPP:2264`. After init it calls `MainGameMenu(load)`
(`PERSO.CPP:2737`), which eventually calls `MainLoop()` (`PERSO.CPP:429`). The tick loop
is `while (TRUE)` at `PERSO.CPP:469`, closing at `PERSO.CPP:1911`. One iteration of that
loop is one rendered frame ("tick" in the CLI sense).

Order within one iteration:

1. **Cube change** — if `NewCube != -1`, `ChangeCube()` runs (`PERSO.CPP:494`/`539`,
   calling `OBJECT.CPP:1157`). This is how `--load` and the `cube`/`load` console
   commands take effect: they set `NewCube`/`FlagLoadGame` and the *next* iteration
   performs the scene swap.
2. **Input poll + time** — `MyGetInput()` then `ManageTime()` (`PERSO.CPP:572-573`).
   `ManageTime()` (`LIB386/SYSTEM/TIMER.CPP:72`) reads `SDL_GetTicks()` and advances the
   game clock `TimerRefHR` by the *wall-clock* delta since last call. This is the crux of
   the determinism story (§7).
3. **Debug / console key handling** — `PERSO.CPP:603-771`, including the pending
   screenshot drain at `PERSO.CPP:763-771`.
4. **Game input actions** — help/menu/save/load/pause/inventory, gated by `!FlagFade`
   (`PERSO.CPP:776-1461`). The interactive in-game load lives here (`PERSO.CPP:914-973`)
   and is the template for what `--load` does non-interactively.
5. **Per-frame world step** — `GereAmbiance()`/`CheckNextMusic()` (`PERSO.CPP:1477`),
   `GereExtras()`/`AnimAllFlow()`/rain (`PERSO.CPP:1489-1497`), then the **main actor
   loop** (`PERSO.CPP:1499-1626`): per object `DoDir()` (`:1539`), `DoTrack()` = move/track
   script (`:1546`, `SOURCES/GERETRAK.CPP:140`), `PtrDoAnim()` (`:1550`), `CheckZoneSce()`
   (`:1553`), `DoLife()` = life script (`:1590`, `SOURCES/GERELIFE.CPP:742`). If a script
   sets `NewCube`, it `goto startloop` (`:1623`).
6. **Camera recenter** — follow-camera and snap logic (`PERSO.CPP:1631-1862`).
7. **Render + present** — `AffScene(FirstTime)` (`PERSO.CPP:1878`, `OBJECT.CPP:5298`).
   `AffScene` clears, draws terrain, walks objects into the sort tree (`TreeInsert`),
   draws front-to-back, and flips. The actual pixel present is triggered by
   `BoxUpdate()`/the flip path → `UnlockVideoSurface()` → `PresentFrame()`
   (`LIB386/SVGA/SDL.CPP:196`), which converts the 8-bit `Log` buffer through the palette
   LUT to an ARGB texture, calls the pre-present callback, and presents.
8. **Frame tail** — music kick, `FirstTime = AFF_OBJETS_FLIP`, `CmptFrame++`
   (`PERSO.CPP:1886-1900`).

Note the order is **input → scripts/physics → render** (not the classic
input→logic→render→present as separate phases). The world simulation step (5) and the
render (7) are distinct, but both live inside the one loop body and there is **no
function that advances exactly one tick** — the loop is monolithic with `goto startloop`
/ `goto restartloop` labels (`:470`, `:437`). A `--tick N` driver therefore has to either
re-enter this loop body N times or run `MainLoop()` with an injected exit condition.

Where `--tick N` injects cleanly: the natural seam is the **top of the loop body**, right
after the cube-change block and before/around `MyGetInput()` (`PERSO.CPP:570`). A counter
that decrements once per iteration and breaks the loop is the least invasive shape. It
does not skew the game clock by itself, but see §7: the clock advances by real elapsed
wall time per iteration regardless, so "N ticks" is N frames of *rendering*, each
consuming an unpredictable amount of game-time unless the timer is pinned.

I'm **not certain** the loop can be safely run for N iterations from a cold load without
at least one full `AffScene` having run first — see the first-frame hazards in §3.

---

## 2. Console subsystem

There is a full Quake-style console: `SOURCES/CONSOLE/`, documented in `docs/CONSOLE.md`.
It is **always compiled** (no CMake flag) — `SOURCES/CMakeLists.txt:37` adds
`CONSOLE_CMD.CPP` unconditionally, `:107-109` link the `console` library. Toggle key is
F12 by default (`CONSOLE.CPP:78`).

**Command execute entry point (this is the `--exec` hook):**
`void Console_Execute(const char *line)` at `SOURCES/CONSOLE/CONSOLE.CPP:565`, declared in
`CONSOLE.H:70` and already described there as "Internal command execution entry point;
used by host-only parser tests." It tokenises the line, tries cheats
(`TryExecuteCheatByName`), then built-ins (`help`/`cmdlist`/`varlist`), registered
commands, then cvar get/set. It does **not** require the console to be open or visible —
the host parser tests call it directly. A `;`-separated `--exec "a;b"` would just be split
and fed line by line.

**Registration API:** `Console_RegisterCommand` / `Console_RegisterCommandEx`
(`CONSOLE.CPP:525`/`:514`), `Console_RegisterCvar` (`:529`). All registration happens in
`Console_RegisterAll()` (`CONSOLE_CMD.CPP:924`), invoked lazily on first console open
(`CONSOLE.CPP:148`). **Hazard for `--exec`:** if the console is never toggled open,
`Console_RegisterAll()` is never called and the command table is empty. The harness must
call `Console_RegisterAll()` (guarded by the `s_registered` flag it already sets) before
`Console_Execute`. Limits: `CONSOLE_MAX_CMDS 32` (`CONSOLE.CPP:24`) — currently 23
commands registered, so there is headroom but not much.

**Every console command found** (handlers in `CONSOLE_CMD.CPP`, registered at `:924-973`):

| Command | Line | Effect |
|---|---|---|
| `cube <n>` | `:67` | Sets `NewCube` → cube change next frame |
| `load <name>` | `:77` | Resolve save by player name or filename, `FlagLoadGame=TRUE`, `LoadGameNumCube()` |
| `loadbug [name]` | `:173` | Load a bug save |
| `savebug [name]` | `:205` | Save current game into bugs dir |
| `timer [ms]` | `:243` | `SetTimerHR(TimerRefHR + ms)` (default 200) |
| `status` | `:249` | Island/cube/chapter, obj/zone/track counts, FPS, `TimerRefHR`, hero pos |
| `screenshot` | `:264` | `Console_RequestScreenshot(path, png=1)` |
| `give <item> [count]` | `:275` | Grant inventory item / money / clover |
| `playvideo <name>` | `:343` | Play ACF video |
| `credits [0\|1]` | `:355` | Credits sequence |
| `slide <name>` | `:421` | Distributor/bumper slide |
| `distrib <name>` | `:517` | Set distributor version for next launch |
| `listvideos` | `:567` | List ACF names |
| `playjingle <1-26>` | `:575` | Play jingle |
| `listjingles` | `:585` | List jingles |
| `playmusic <num> [loop]` | `:590` | Play music track |
| `listmusic` | `:604` | List music |
| `playsample <idx> [...]` | `:609` | Play sample |
| `listsamples` | `:631` | List samples |
| `audio sample\|music\|global ...` | `:673` | Audio subcommands |
| `video log <0\|1>` | `:821` | Smacker video logging |
| `buildinfo` | `:809` | Version, build timestamp, CMake flags |
| `exit` | `:842` | Exit immediately |
| `listsaves` | `:849` | List saves by player name |
| `listbugs` | `:896` | List bug saves |

Built-ins not in the table: `help [topic]`, `cmdlist`, `varlist` (`CONSOLE.CPP:594-661`).
Cvars (`:971-973`): `fps`, `horizon`, `zv`. Cheats are a separate name table consulted
first (`TryExecuteCheatByName`, `SOURCES/CHEATCOD.CPP`).

Console output goes to an in-memory scrollback ring (`s_scrollback`,
`CONSOLE.CPP:52`) via `Console_Print` (`:233`). There is a test sink hook
`Console_SetLineSinkForTests` (`:263`) that receives every pushed line — this is a ready
path for capturing command output for `--dump-state` or for echoing to stdout.

**Smallest thing to add for `--exec`:** nothing structural — call `Console_RegisterAll()`
then `Console_Execute()` per command. The console already is the command parser.

---

## 3. Save/load path

The restore sequence used everywhere is **two steps**:

1. `InitGame(-1)` (`SOURCES/PERSO.CPP:291`, the `argc==-1` branch at `:333`) — resets game
   lists/hero/holomap, sets `FlagLoadGame=TRUE` and calls `LoadGameNumCube()`.
2. `ChangeCube()` (`SOURCES/OBJECT.CPP:1157`) — performs the actual scene/cube load
   (`LoadScene`, `:1217`), object init, grid build.

`LoadGameNumCube()` (`SOURCES/SAVEGAME.CPP:635`) reads the `.lba` file into `Screen`,
parses version + `NewCube` + player name, then restores `ListVarGame[]` (the script
variable array). It does **not** load the scene — it only sets `NewCube` and the var
state; `ChangeCube()` consumes `NewCube`.

`GamePathname` (global) is the input: a console `load` resolves it from a name
(`CONSOLE_CMD.CPP:103-165`); `main()` resolves it from `ArgV[1]` today
(`PERSO.CPP:2669-2702`).

**There is already a non-interactive harness that does exactly this**, added for issue #62:
the `--save-load-test <path>` flag (`PERSO.CPP:2248-2261` parse, `:2708-2735` drive). It
sets `GamePathname`, calls `InitGame(-1)` then `ChangeCube()`, prints parseable
`SAVE_LOAD_TEST:` lines, and exits — without ever entering `MainLoop()`. This is the
closest existing precedent to the proposed harness and the model for `--load … --exit`.

**Safe before the main loop:** `InitGame(-1)` + `ChangeCube()` are safe pre-loop — the
existing harness proves it (it never ticks). After them, `NumCube`, `NbObjets`,
`NbPatches`, `ListVarGame[]`, and the actor list are populated.

**First-frame hazards (requires a tick / an `AffScene` to be valid):**

- The interactive load path (`PERSO.CPP:932-958`) does **more** than `InitGame(-1)` +
  `ChangeCube()`: it then calls `AffScene(AFF_ALL_NO_FLIP)`, `CopyScreen(Log, Screen)`,
  `CurrentSaveGame()`, and `goto restartloop`. So a fully "settled" loaded frame — one
  whose framebuffer is renderable and whose camera is centred — needs at least one
  `AffScene`. A `--screenshot` taken immediately after load with no tick would capture an
  uninitialised/black framebuffer.
- `ChangeCube()` reseeds RNG from the clock (`OBJECT.CPP:1171`, see §7).
- `MainLoop()` sets `FirstLoop=TRUE` for the first cube iteration (`PERSO.CPP:542`); some
  per-object work is gated on `!FirstLoop` (e.g. `PtrDoAnim`, `PERSO.CPP:1549`). So the
  first tick is not identical to subsequent ticks.
- The follow-camera state has explicit "first-frame init" branches
  (`PERSO.CPP:1675-1685`) that only settle after a frame.

**Implication for the CLI:** `--load` can restore before the loop, but `--screenshot`
and a meaningful `--dump-state` want at least one `--tick` to have run so the scene is
rendered and the camera/animation state has settled. The plan should make `--tick`
default to ≥1 when `--screenshot` is present, or document the black-frame caveat. I'm
**not certain** how many ticks it takes for animation interpolation to reach a stable
pose — likely scene-dependent.

---

## 4. Argument parsing

There is **no central arg registry** — parsing is ad-hoc, split across three places:

1. **Raw `argv` in `main()`** before SDL/engine init: `--version` (`PERSO.CPP:2273`) and
   `--save-load-test` (`parse_save_load_test_flag`, `PERSO.CPP:2248`). The latter is the
   pattern to copy: walk `argv`, pull recognised flags + their values into globals, and
   **compact `argv` in place** so downstream parsing doesn't choke.
2. **`ResolveGameDataDir(...&argc, argv)`** (`PERSO.CPP:2305`) consumes `--game-dir`.
3. **`InitAdeline(argc, argv)`** (`PERSO.CPP:2330`) → `GetCmdLine()`
   (`LIB386/SYSTEM/CMDLINE.CPP:118`) populates the engine globals `ArgC`/`ArgV`
   (`CMDLINE.H:11-12`, max 30 args). Remaining positional args are read later: a bare
   numeric `ArgV[1]` selects a cube under `DEBUG_TOOLS` (`PERSO.CPP:2645`), otherwise
   `ArgV[1]` is treated as a save name (`PERSO.CPP:2669`). `FindAndRemoveParam()`
   (`CMDLINE.CPP:101`) is the engine's flag-lookup helper.

**Where new flags go:** the cleanest hook for the new flags is a single
`parse_automation_flags(&argc, argv)` called next to `parse_save_load_test_flag`
(`PERSO.CPP:2285`), pulling `--load/--exec/--tick/--dump-state/--screenshot/--exit` into a
small config struct before the engine sees them. This mirrors existing precedent exactly.
Flags that need values (`--load`, `--exec`, `--tick`, paths) follow the `--save-load-test`
two-token pattern.

Caveat: the `--exec` value contains spaces and `;`. It must be a single quoted argv token
(`--exec "give magicball; cube 5"`) — the in-place compaction handles that fine since
each is one `argv[i]`. The `CMDLINE.CPP` quote handling only matters for the in-engine
`CmdLine` config string, not for OS `argv`.

---

## 5. Screenshot capability

**It exists and works.** `void SavePNG(char *filename, void *screen, S32 xres, S32 yres,
void *ptrpalette)` — `LIB386/FILEIO/SAVEPNG.CPP`, declared `LIB386/H/FILEIO/SAVEPNG.H`.
It takes the 8-bit indexed buffer + palette and writes a PNG via `stb_image_write`.

How it's triggered today (console `screenshot` command):

1. `cmd_screenshot` (`CONSOLE_CMD.CPP:264`) calls `Console_RequestScreenshot(path, png=1)`.
2. That stashes the path and closes the console (`CONSOLE.CPP:107`).
3. Next frame, the main loop drains it: `Console_HasPendingScreenshot(...)` →
   `SavePNG(shotPath, Screen, ModeDesiredX, ModeDesiredY, PtrPal)` (`PERSO.CPP:763-771`).

There is also a no-dependency **PPM** fallback written directly from the ARGB framebuffer
inside the pre-present callback `Console_PrePresent` (`CONSOLE.CPP:395-418`) when
`png=0`. This proves framebuffer read-back works at the present boundary.

Present/read-back details: `PresentFrame()` (`LIB386/SVGA/SDL.CPP:196`) reads the 8-bit
`Log` buffer (`ModeDesiredX × ModeDesiredY`), converts via `paletteLUT` to an ARGB
texture, then calls `s_prePresent(...)` with the ARGB pixels + palette
(`SDL.CPP:217-226`) before presenting. So two read-back points exist: the indexed
`Screen`/`Log` buffer (what `SavePNG` uses) and the composited ARGB texture (what the PPM
path uses). SDL3 is the active backend (`SDL.CPP`); there is no SDL2 path in the live
build.

**For the CLI:** `--screenshot <path>` can reuse `SavePNG` directly. The only nuance is
*when* — it should fire after the post-`--tick` `AffScene`, reading `Screen` (or `Log`)
with `PtrPal`. The existing pending-screenshot drain at `PERSO.CPP:763` is a usable seam if
we want it routed through the loop; a direct `SavePNG` call from the harness after the tick
loop is simpler. `Screen` vs `Log`: the loop copies `Log→Screen` in several places; the
console screenshot uses `Screen`, so that's the safer default.

---

## 6. Engine state worth dumping

A developer watch window maps to these. All are globals (C linkage), reachable from a
harness TU that includes the right headers.

**Hero + actors** — `ListObjet[]`, array of `T_OBJET` (`SOURCES/DEFINES.H:340-449`); hero
is `ListObjet[NUM_PERSO]`, `NUM_PERSO == 0` (`SOURCES/COMMON.H:902`). Live count is
`NbObjets`. Per-actor fields worth dumping:
- Embedded `T_OBJ_3D Obj` (`LIB386/H/OBJECT/AFF_OBJ.H`): world `X/Y/Z`, local angles
  `Alpha/Beta/Gamma`, `Body.Num`, `Anim.Num`, `LastFrame`/`NextFrame`/`NbFrames`,
  `LoopFrame`. (`T_PTR_NUM` is the `void*`/`S32` union — dump the `.Num` view.)
- `LifePoint`, `Move`, `GenAnim`/`GenBody`, `FlagAnim`, `OffsetLife`, `OffsetTrack`,
  `LabelTrack`, `ZoneSce`, `Flags`, `WorkFlags`, `CarryBy`, `MemoComportement`.

**Scene / island** — `Island`, `NumCube`, `CubeMode` (interior/exterior),
`NbObjets`, `NbZones`, `NbBrickTrack`, `NbPatches`. The world hero position globals
`Nxw/Nyw/Nzw` (used by `cmd_status`, `CONSOLE_CMD.CPP:261`).

**Hero behaviour / inventory** — `Comportement` (current behaviour, a global, not just the
per-actor field), `Weapon`, `MagicLevel`, `MagicPoint`, money `NbGoldPieces` /
`NbZlitosPieces`, `NbLittleKeys`, `NbCloverBox`. Inventory possession is in `ListVarGame[]`
(`give` writes `ListVarGame[index]`, `CONSOLE_CMD.CPP:320`).

**Script variables** — `ListVarGame[]`, size `MAX_VARS_GAME == 256`
(`SOURCES/COMMON.H:479`). This is the array `LoadGameNumCube` restores
(`SAVEGAME.CPP:666`). I'm **not certain** there is a separate per-scene/per-actor "track
var" array distinct from `ListVarGame`; worth a closer look in Phase 2 before claiming the
dump is complete (the LBA engine historically also had per-actor `Info`/`Info1..3` scratch
in `T_OBJET`, `DEFINES.H:353-356`).

**Camera** — `BetaCam`, `AlphaCam`, `VueDistance`, `VueOffsetX/Y/Z`,
`StartXCube/YCube/ZCube` (`PERSO.CPP:1735-1741`, see `docs/CAMERA.md`).

**Timer / frame** — `TimerRefHR`, `CmptFrame`, `NbFramePerSecond` (all in
`LIB386/SYSTEM/TIMER.CPP:15-18`).

**Recent log lines** — two sources: the console scrollback ring (`s_scrollback`,
`CONSOLE.CPP:52`; would need a small getter, none exists yet) and `LogPrintf`
(`LIB386/SYSTEM/LOGPRINT.CPP:39`) which writes to a logfile, not a ring buffer. The
test-sink hook `Console_SetLineSinkForTests` (`CONSOLE.CPP:263`) is the cleanest existing
tap for capturing command output during `--exec`.

`cmd_status` (`CONSOLE_CMD.CPP:249`) already assembles the island/cube/chapter/counts/FPS/
pos subset — it's effectively a one-line text version of the proposed dump and a good
reference for which globals are reliably populated.

---

## 7. Determinism hazards

The same `--load X --exec Y --tick N` will **not** reproduce byte-identical `state.json`
across runs today, before any harness work. The reasons, in priority order:

1. **RNG is seeded from the wall clock.** `ChangeCube()` does `srand(TimerRefHR)`
   (`OBJECT.CPP:1171`). `TimerRefHR` is accumulated wall-clock ms (next item), so the seed
   differs every run. `Rnd(n)` is `rand() % n` (`LIB386/H/SYSTEM.H:59`), `MyRnd` wraps it
   (`COMMON.H:9`). RNG drives particle flows (`FLOW.CPP`), ambiance/sample selection
   (`AMBIANCE.CPP`), texture animation (`ANIMTEX.CPP`), and some script branches
   (`GERELIFE.CPP`, `GERETRAK.CPP`). **This is the single biggest hazard** and also the
   easiest future lever: pin `TimerRefHR` before `ChangeCube` and the seed becomes fixed.
2. **The game clock is wall-clock driven, not fixed-step.** `ManageTime()`
   (`TIMER.CPP:72-79`) advances `TimerRefHR` by `SDL_GetTicks()` deltas each iteration.
   Movement speed (`GetDeltaMove`, `LIB386/3D/MOVE.CPP`, via `StepFalling`/`StepShifting`,
   `PERSO.CPP:1466-1468`) and animation interpolation (`LIB386/ANIM/FRAME.CPP`,
   `INTERDEP.CPP`) are functions of elapsed game-time. So "N ticks" consumes a
   nondeterministic amount of simulated time — a fast machine advances less per frame than
   a slow one. There is a lever here too: `LockTimer()`/`SetTimerHR()` (`TIMER.CPP:38`,
   `:66`) already let code freeze and set the clock; a fixed per-tick increment would make
   the step deterministic. (Out of scope to fix now — flagging only.)
3. **Audio-driven script branches.** Some logic gates on `IsSamplePlaying(...)`
   (e.g. `PERSO.CPP:1560`, `:1475`). With the null audio backend
   (`-DSOUND_BACKEND=null`) these are stable; with real audio they depend on sample
   timing. The harness should prefer the null backend for reproducibility.
4. **Threading.** The SDL audio backend services samples on a callback thread
   (`LIB386/AIL/SDL/SAMPLE.CPP`). State the dump reads is on the main thread, but audio
   timing feeding item 3 is not. Null backend sidesteps this.
5. **`FirstLoop` asymmetry.** The first post-load tick differs from later ticks
   (`PERSO.CPP:542`, `:1549`) — not nondeterministic per se, but means tick 1 ≠ tick 2 for
   the same state, so the dump must record the tick count.

None of these are fixed in this pass (per spec). The plan should (a) recommend the null
audio backend for harness runs, (b) note that a future "deterministic mode" can pin
`TimerRefHR` + reseed `srand` with a fixed value, and (c) ensure the CLI design doesn't
*preclude* that pinning — it doesn't, since the timer hooks already exist.

---

## 8. Polyrec hooks (scoping for later — do not build)

Today's polyrec (`docs/POLYREC.md`, `ENABLE_POLY_RECORDING`) records **polygon draw
calls**, not input — capture starts in `AffScene` (`OBJECT.CPP:5320`) on Alt+F9
(`PERSO.CPP:752-759`) and writes `.lba2polyrec` files. That is a *render-side* record, a
different axis from input record/replay.

For **input** record/replay, the hook point is the input poll at the top of the loop:
`MyGetInput()` (`PERSO.CPP:572`, defined `SOURCES/INPUT.CPP:258`). Recording taps the
input state right after that call; playback stubs/overrides it (or overwrites the globals
immediately after) before the scripts/physics step at `PERSO.CPP:1499` consumes it.

The input state the engine actually reads downstream:
- `Key` / `MyKey` — current keyboard scancode (`INPUT.CPP:263`).
- `Input` — bitmask of derived game actions (`I_FIRE`, `I_MENUS`, `I_CAMERA`, … in
  `SOURCES/INPUT.H`), computed by `GetInput()` from key state + gamepad bindings.
- `TabKeys[]` — per-scancode pressed array (raw), plus joystick axes folded in at
  `TabKeys[256]` (`INPUT.CPP:259`).

Smallest faithful round-trip struct, per tick: `{ U32 tick; S32 Key; U32 Input; }` is
enough to replay *gameplay* deterministically **if** we also override `MyGetInput` to stop
re-deriving from hardware on replay (otherwise live input would clobber it). If we want
menu/camera/raw-key fidelity too, add the small set of `TabKeys[]` indices the loop reads
directly (e.g. the `K_LBRACKET`/`K_RBRACKET` pan keys, `PERSO.CPP:1672`) — but the derived
`{Key, Input}` pair is the minimum and is what the gameplay/script layer consumes.

**Does the proposed CLI close the input-replay door?** No, and this is the key check the
spec asked for:
- `--tick N` needs a top-of-loop seam (§1). That same seam is exactly where input
  record/playback hooks in. Designing `--tick` around a clean per-iteration step *helps*
  polyrec rather than blocking it.
- `--exec` driving the console writes through the same globals (`NewCube`, `FlagLoadGame`,
  `ListVarGame`) that a replay would; no conflict.
- The determinism levers in §7 (pin timer, fix seed) are *prerequisites* for faithful
  input replay anyway, so flagging them now is aligned.

The one thing to avoid: building `--tick` as "call `MainLoop()` and let it run free,"
which would give no per-iteration seam. A per-tick driver (decrement-and-break, or a
re-enterable step) keeps the door open.

---

## Summary of what's reusable vs new

| Need | Reusable today | New work |
|---|---|---|
| `--exec` | `Console_Execute` (`CONSOLE.CPP:565`), full command set | Call `Console_RegisterAll()` first; split on `;` |
| `--load` | `InitGame(-1)` + `ChangeCube()` (proven by `--save-load-test`) | Set `GamePathname` from flag; sequence before loop |
| `--screenshot` | `SavePNG` (`SAVEPNG.CPP`), pending-shot drain (`PERSO.CPP:763`) | Fire after post-tick `AffScene`; default ≥1 tick |
| `--tick N` | The loop body itself (`PERSO.CPP:469-1911`), timer hooks | Per-tick step driver / re-enterable seam |
| `--dump-state` | `cmd_status` as a field reference; all globals exist | JSON writer; pick field set; log-ring getter |
| `--exit` | `cmd_exit` (`CONSOLE_CMD.CPP:842`), `TheEnd()` | Clean exit after the above |
| arg parsing | `parse_save_load_test_flag` pattern (`PERSO.CPP:2248`) | `parse_automation_flags`, CMake `LBA2_AUTOMATION` |

Biggest open risks for Phase 2: (1) the lack of a clean single-tick step function means
the `--tick` driver design needs care; (2) determinism is genuinely broken today, so
"dump-state is reproducible" cannot be promised without the null audio backend and, later,
timer/seed pinning; (3) the first-frame hazards mean `--screenshot`/`--dump-state` are only
meaningful after at least one rendered tick.
