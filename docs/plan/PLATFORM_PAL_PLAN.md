# In-place Platform Abstraction Layer (PAL): Audit & Extraction Plan

**Status:** Research + plan only. No code, no build changes, no file moves. A proposal
awaiting explicit go/no-go on the §6 decisions before any implementation PR.

**Scope:** Ground RFC [#120](https://github.com/LBALab/lba2-classic-community/discussions/120)
in the actual codebase and produce an in-place plan that decouples the engine from
direct SDL3 **at the call-site level**, without the Phase 0 directory restructure. New code
lives in `LIB386/PLATFORM/`. The one candidate move is a *targeted* relocation of the
SDL-owning files into that directory (see §3.6), explicitly **not** the Phase 0 restructure,
and a separate go/no-go from routing the seam.

**Evidence tags:** `[verified-from-code]` (read the exact lines), `[verified-from-docs]`
(checked against another doc in this repo), `[from-RFC]` (asserted by Discussion #120, not yet
code), `[assumed]` (inference / estimate), `[design]` (a design choice or recommendation, not a
code finding). All `path:line` citations are against the tree at audit time.

---

## 0. Decision-ready summary

**Is the loop-inversion gate (gate-0) clear? YES, and it is already three-quarters
proven.** The engine does **not** need to learn to be stepped from scratch: a fixed-dt
deterministic clock and a per-tick CLI harness seam already exist and ship today.

- `[verified-from-code]` `MainLoop()` is a single `while (TRUE)` at `SOURCES/PERSO.CPP:478`
  (function spans 438–~1962). It already calls a per-tick hook `Control_TickHook()` at
  `SOURCES/PERSO.CPP:568`, and the whole engine can be driven in discrete, deterministic
  ticks via the `Control_*` harness (`SOURCES/CONTROL.CPP:339,393,667`) backed by a
  fixed-dt virtual clock (`LIB386/SYSTEM/TIMER.CPP:90–138`). **Discrete stepping is not a
  hypothesis here; it is exercised in CI.** The remaining work to expose a clean
  `Step_OneFrame()` is mechanical (hoist the loop body; convert `goto`/`break`/`return`
  exits to a state flag).
- `[verified-from-code]` **The SDL surface is already remarkably contained.** The event
  pump is a single function (`LIB386/SYSTEM/EVENTS.CPP:65`). Audio is fully behind a
  build-time backend seam with a complete NULL backend (`LIB386/AIL/`,
  `-DSOUND_BACKEND=null`). Time has a single seam (`ManageTime()`,
  `LIB386/SYSTEM/TIMER.CPP:144`). Input already funnels through one bit-packed array
  (`TabKeys[]`), and PR [#58](https://github.com/LBALab/lba2-classic-community/pull/58) proved the "virtual scancode" pattern that lets gamepad ride
  the same seam as keyboard.
- `[verified-from-code]` **Audio is a non-issue for the PAL.** AIL already *is* the audio
  abstraction (SDL / NULL / MILES backends). SDL audio symbols appear **only** under
  `LIB386/AIL/SDL/`. The RFC's "audio is not in the PAL" is already true in the code; the
  "AIL → PAL sink" retarget is **unnecessary** for behavior preservation.

**Recommended sequencing decision (the headless-stepper chicken-and-egg):** Prove
loop-structure inversion **first** as PR-0 (extract `Step_OneFrame()`; it may still call
SDL indirectly), because the harness already de-risks it. Land the *running*
`platform_headless` stepper **after** the input + ticks + video subsystems are behind
`g_platform` (PR-6), so the headless backend has real seams to plug into rather than
stubs that drift.

**Headline risk items** (detailed in §3.4 and §6): (1) `present_frame()` as specified in
the RFC fights the engine's actual present model (pull-from-global-buffer + lock-count
gate + engine-owned dirty boxes); (2) the RFC's raw-axis input pull (`axis_value`,
`button_down`) is **dead on arrival**: the engine never sees raw axes, PR #58 digitizes
them into virtual scancodes; (3) quit is a hardcoded `exit(0)` (`LIB386/SYSTEM/WINDOW.CPP:454`)
with no quit flag, so `should_quit()` has nothing to read yet; (4) the engine's key
constants *are* SDL scancodes (`K_X ≡ SDL_SCANCODE_X`), a header-level coupling the guard
ratchet must explicitly carve out or the PAL must redefine; (5) **"no moves" and a guard that
flips to *error* are in tension**: in-place forwarding leaves the bulk of SDL in
`LIB386/SYSTEM`/`SVGA`, so true containment needs either a targeted move of the SDL-owning
files into `LIB386/PLATFORM/` or a downgrade of the guard's goal to regression-prevention
(see §3.6). Consequently the in-place seam decouples *call sites*, not the *link graph*:
`platform_sdl` still depends on `system`.

**Bottom line:** The plan is viable and lower-risk than the RFC assumes, *because the
hardest piece (deterministic discrete stepping) already exists*. The PAL is mostly a
matter of routing already-centralized seams through one `g_platform` indirection and
adding a headless backend. Proceed, eyes open that in-place buys the routing seam, the
headless backend, and a regression guard *now*; full physical SDL containment (and the RFC's
`platform → nothing internal` dependency rule) still requires the targeted move in §3.6.

**Alignment with the existing architecture docs `[verified-from-docs]`.** Cross-checked against
the three layer/membrane docs; consistent, no load-bearing contradiction:
- `ARCHITECTURE.md` already names the platform seam **"already underway"** (its strangler-fig
  roadmap step 3, citing PR [#284](https://github.com/LBALab/lba2-classic-community/pull/284)–[#286](https://github.com/LBALab/lba2-classic-community/pull/286)) and lists *Platform / IO* as a first-class domain; this
  plan is the **continuation** of that named work, engaging the doc's *layer* axis (the PAL) and
  *time* axis (loop inversion).
- `ENGINE_GAME_SEAM.md` already labels `SYSTEM`/`SVGA` + the community port layer (`CONTROL`,
  `RES_*`, `TOUCH_INPUT`) "the PAL" (`:266`), pins `AIL/SDL` as the audio backend (confirms §1.3)
  and the dormant register (confirms §5). Its one caveat, that `engine·platform` hybrids are
  *heritage* and you move only when it "pays off twice" (`:270–287`), is reconciled in §3.6.
- `ENGINE_GAME_INTERFACE.md` confirms the PAL is **orthogonal** to the Life/Track script VM:
  scripts reach the engine only through opcode handlers, never platform code, so routing
  input/timing/present through `g_platform` never crosses the engine/game membrane.

---

## 1. Audit findings

### 1.1 Loop ownership (gate-0) `[verified-from-code]`

**Entry point:** `main()` at `SOURCES/PERSO.CPP` (~2399). After init it branches: under the
CLI harness it runs `Control_Begin()` → `MainLoop()` → `Control_End()`
(`SOURCES/PERSO.CPP:2979–2982`); in normal play it enters via the game menu.

**The loop:** `S32 MainLoop()` at `SOURCES/PERSO.CPP:438`, body is `while (TRUE)` at `:478`.
Per-iteration order (representative):

```
Perftrace_FrameBegin()                         ; PERSO.CPP:~481
(scene/cube load if NewCube != -1)             ; PERSO.CPP:~512
Control_TickHook()                             ; PERSO.CPP:568   <-- existing per-tick seam
MyGetInput()                                   ; PERSO.CPP:594   <-- input pump
ManageTime()                                   ; PERSO.CPP:595   <-- time seam
(input dispatch: menus, debug, hotkeys)        ; PERSO.CPP:~600–1512
(object loop: DoDir/DoTrack/PtrDoAnim/DoLife)  ; PERSO.CPP:~1537–1678
(camera state machine)                         ; PERSO.CPP:~1683–1923
AffScene(FirstTime)                            ; PERSO.CPP:1929  <-- render/present
Perftrace_FrameEnd()
```

**Timing is external to the loop** `[verified-from-code]`: no `SDL_Delay` in the hot loop;
pacing is the renderer's vsync (`SDL_SetRenderVSync`, `LIB386/SYSTEM/WINDOW.CPP:196,414`).
`ManageTime()` (`LIB386/SYSTEM/TIMER.CPP:144`) is the single clock seam and already reads a
virtual clock when fixed-dt is armed (see §1.6). This is *ideal* for inversion: the caller
owns cadence.

**State is global** `[verified-from-code]`: game state is `extern`/`static`
(`SOURCES/C_EXTERN.H`, `SOURCES/PERSO.CPP:60–81`). So an external `Step_OneFrame()` needs no
parameter marshalling. Cost: no two engine instances in one process (irrelevant here).

**Obstacles to extracting `Step_OneFrame()`** `[verified-from-code]`: control flow leaves
the loop body four ways; all are mechanical to convert:

| Construct | Sites | Conversion |
|---|---|---|
| `goto restartloop` (label at `:446`, **outside** the `while`) | `:552`, `:1010` | set `loopState=RESTART; return;` and run the restart-init block at the top of the next step |
| `goto startloop` (label at `:479`, **inside** the loop, restart current frame) | `:1676` (verify exact line at implementation) | stays internal to `Step_OneFrame()` (a local loop or early continue) |
| `break` (normal menu/ESC exit) | `:879` and a cluster `:1102–1571` | set `exitCode=…; return;` |
| early `return` | `:520`, `:632`, `:663`, `:1646` (`return FlagTheEnd; // mmm violent`) | set `exitCode=…; return;` |

Modal sub-menus (`MenuConfig`, `ChoosePlayerName`, …) are **already self-contained**:
they run their own inner loop calling `MyGetInput()`/`ManageTime()`/`AffScene()` and return
(`[verified-from-code]` corroborated by the many nested `MyGetInput()`/`ManageTime()` sites
at `:677–1060`). They need **no** change for inversion. (The engine/game *membrane* runs
**inside** this loop: the Life/Track script VM at `DoLife`/`DoTrack`, i.e. `GERELIFE`/`GERETRAK`;
see `ENGINE_GAME_SEAM.md` and `ENGINE_GAME_INTERFACE.md`. `MainLoop()` owns the loop *structure*;
the VM owns the engine/game seam; the PAL touches neither, routing only the platform I/O around
them.)

**Estimate** `[assumed]`: ~250–400 lines touched in `PERSO.CPP`, zero behavior change,
verifiable against the existing fixed-dt demo determinism tests.

### 1.2 SDL coupling: the migration surface `[verified-from-code]`

Authoritative set of **non-build, non-test** source/header files that reference `SDL_` or
`#include <SDL`. Grouped by role:

**Group A, per-frame engine seams (the PAL proper):**
- Video/present: `LIB386/SVGA/SDL.CPP`, headers `LIB386/H/SVGA/VIDEO.H`,
  `LIB386/H/SVGA/STAGING_BUFFER.H`
- Window/events: `LIB386/SYSTEM/WINDOW.CPP`, `LIB386/SYSTEM/EVENTS.CPP`, headers
  `LIB386/H/SYSTEM/WINDOW.H`
- Time: `LIB386/SYSTEM/TIMER.CPP`, header `LIB386/H/SYSTEM/TIMER.H`
- Input: `LIB386/SYSTEM/KEYBOARD.CPP`, `LIB386/SYSTEM/KEYBWIN.CPP`,
  `LIB386/SYSTEM/MOUSE.CPP`, `SOURCES/JOYSTICK.CPP`, `SOURCES/TOUCH_INPUT.CPP`(+`.H`),
  headers `LIB386/H/SYSTEM/KEYBOARD_KEYS.H`, `LIB386/H/SYSTEM/KEYBWIN.H`

**Video-seam halves `[design]`:** Group A's video work splits by responsibility.
`SVGA/SDL.CPP` is the 8-bit indexed framebuffer + palette (the `Video*` API,
`Phys`/`ModeResX`); `WINDOW.CPP` is the SDL window + renderer + present. Since
PR #326, `WINDOW.CPP` also owns the window-to-framebuffer coordinate mapping
(`SDL_RenderCoordinatesFromWindow` + clamp) and lets SDL own the present rect
(`SDL_SetRenderLogicalPresentation`, letterbox). These two files are the video
PAL seam's two halves; settle their naming and boundary at seam-definition time,
not ad hoc. In particular, do not rename `WINDOW.CPP` to `VIDEO.CPP` standalone:
`Video*` already denotes the framebuffer layer.

**Group B, host services (one-shot, not per-frame):**
- Paths / data discovery: `SOURCES/DIRECTORIES.CPP`, `SOURCES/RES_DISCOVERY.CPP`(+`.H`),
  `SOURCES/RES_PICKER.CPP`(+`.H`), `SOURCES/RES_SWITCH.CPP`
- Runtime resolution switch: `SOURCES/RES_SWITCH.CPP:217` (`SDL_SetWindowSize`),
  `:524` (`SDL_GetTicks`)
- Init / sysinfo: `SOURCES/INITADEL.C` (`SDL_GetNumLogicalCPUCores`, `SDL_GetSystemRAM`,
  `SDL_AUDIODRIVER` hint)

**Group C, "SDL as portable libc / platform probe" (easy; arguably already abstracted):**
- `LIB386/SYSTEM/STRING.CPP` (`SDL_malloc/free/strlen/strncasecmp`)
- `LIB386/SYSTEM/AVAILMEM.CPP` (`SDL_GetSystemRAM`)
- `LIB386/SYSTEM/DISPOS.CPP` (`SDL_GetPlatform`)
- `LIB386/SYSTEM/ANDROID.CPP` (`SDL_system.h`; Android TV detection)

**Group D, diagnostics / dev tooling:**
- `LIB386/SYSTEM/LOG.CPP` (`SDL_SetLogOutputFunction`, `SDL_GetCurrentThreadID`)
- `SOURCES/PERFTRACE.CPP` (`SDL_GetPerformanceCounter/Frequency`, `SDL_GetTicks`)
- `SOURCES/CONSOLE/CONSOLE.CPP` (debug console; `SDL_Event`, `SDL_GetKeyFromScancode`)

**Group E, audio (already behind its own backend seam; NOT the PAL):**
- `LIB386/AIL/SDL/*` only. See §1.3.

`WINSYS.CPP` matched the file scan via comments/includes only (no live `SDL_` symbol on
inspection) `[assumed]`.

**Implication for the guard ratchet:** the "no `SDL_` outside `platform_*.cpp`" rule cannot
be literal on day one; Groups B–E legitimately touch SDL. The final guard must allow-list
`LIB386/PLATFORM/PLATFORM_SDL*.CPP`, `LIB386/AIL/SDL/*`, and a small, explicitly-enumerated
tail (Group C/D), or those must be migrated first. This is detailed in §4 (guard ratchet).

### 1.3 Audio path `[verified-from-code]`: already solved

**AIL is the audio abstraction.** Backend chosen at build time:

```cmake
# CMakeLists.txt:29
set(SOUND_BACKEND "sdl" CACHE STRING "Sound backend to be used")
set(SOUND_BACKEND_OPTIONS null miles sdl)
# LIB386/AIL/CMakeLists.txt:3
if (${SOUND_BACKEND} STREQUAL "null")      add_subdirectory(NULL)
elseif (${SOUND_BACKEND} STREQUAL "miles") add_subdirectory(MILES)
elseif (${SOUND_BACKEND} STREQUAL "sdl")   add_subdirectory(SDL)
endif ()
```

- **Public API** (engine-facing, backend-agnostic): `LIB386/H/AIL/{SAMPLE,STREAM,CD,MIDI,VIDEO_AUDIO}.H`,
  e.g. `PlaySample`, `OpenStream/PlayStream`, `OpenCD/PlayCD`, `StartVideoAudio/PushVideoAudio`, etc.
- **SDL coupling is confined to `LIB386/AIL/SDL/`**: samples use an SDL callback
  (`SDL_OpenAudioDeviceStream` + `SampleStreamCallback`, `SAMPLE.CPP:368–407`); streams/CD/
  video use a push model (`SDL_PutAudioStreamData`, `STREAM.CPP:107`, `VIDEO_AUDIO_SDL.CPP:38`).
- **NULL backend** (`LIB386/AIL/NULL/SOUND_NULL_BACKEND.CPP`) is a complete no-op with
  **zero** SDL references: the headless audio path the RFC describes, already real.
- **Smacker video audio** routes through the AIL `VIDEO_AUDIO` API
  (`SOURCES/PLAYACF.CPP` → `StartVideoAudio`/`PushVideoAudio`), **not** direct SDL.

**Verdict:** "only `AIL/SDL` touches SDL audio" is **already satisfied**. An "AIL → PAL
sink" would add indirection for no behavior benefit. Recommendation: **leave AIL as a
sibling audio contract** (matches the RFC's own "audio is not in the PAL"); headless simply
selects `-DSOUND_BACKEND=null`. (Optional nicety, not required: have the headless build
default `SOUND_BACKEND=null`.)

### 1.4 Input `[verified-from-code]`

- **One event pump:** `ManageEvents()` (`LIB386/SYSTEM/EVENTS.CPP:65`) does
  `SDL_PumpEvents()` then a `SDL_PollEvent()` loop dispatching to
  `HandleEventsTimer/Mouse/Keyboard/Joystick/Touch/Video/Window`. It also already has a
  function-pointer interception hook `s_eventFilter` (`EVENTS.CPP:37,74`, used by the debug
  console).
- **Three direct state-poll sites** (outside the event pump, all in the system layer, not
  game code): `UpdateKeyboardState()` and `GetAscii()` call `SDL_GetKeyboardState`
  (`LIB386/SYSTEM/KEYBOARD.CPP:44–46, 74–75`); `UpdateJoystick()` calls
  `SDL_GetGamepadButton/Axis` (`SOURCES/JOYSTICK.CPP:304–339`). Game code never calls SDL
  input directly.
- **The central input datum is `TabKeys[]`**: a bit-packed key-state array
  (`LIB386/SYSTEM/KEYBOARD.CPP:21`), indexed via `KeyIndex()/KeyBit()` (`:15–16`).
  `CheckKey()`/`GetInput()` read it. Keyboard constants **are SDL scancodes**:
  `#define K_A SDL_SCANCODE_A` (`LIB386/H/SYSTEM/KEYBOARD_KEYS.H`, includes
  `<SDL3/SDL_keycode.h>`). ⚠ This is a header-level SDL coupling baked into the engine's key
  vocabulary.
- **PR #58, the micro-PAL to generalize** `[verified-from-code]`: gamepad input is mapped
  to *virtual scancodes* `K_GAMEPAD_* = K_GAMEPAD_BASE(1024)+n` (`KEYBOARD_KEYS.H`) via a
  **data table + translation function** (no callbacks): `static const ButtonMap kButtons[]`
  in `UpdateJoystick()` (`JOYSTICK.CPP`), with analog sticks digitized to 8-way virtual keys
  by `ApplyStickDirection()`. Those virtual keys are written into the **same `TabKeys[]`
  array** (`SetVirtualKeyDown`), so `CheckKey()`/`GetInput()` consume gamepad input with no
  special-casing. A second rebindable layer (`GamepadKeys[]`, reusing `T_DEF_KEY`) maps
  virtual scancodes → engine action bits in `ApplyGamepadBindings()` (`SOURCES/INPUT.CPP`).
  The LIB386→game edge is inverted with a **weak symbol** `HandleEventsJoystick`
  (`EVENTS.CPP`). **The seam is a data seam (`TabKeys[]`), not an API**: this is the
  pattern the RFC says the PAL "builds directly on."
- **Device-origin flag:** `LastInputWasKeyboard` (`KEYBOARD.CPP:30`, declared
  `SOURCES/C_EXTERN.H`), last-input-wins, set by handlers; read by the save menu.
- **Injection readiness: YES** `[verified-from-code]`: `tests/input_device/test_input_device.cpp`
  already injects deterministically by calling the `HandleEventsXxx` handlers with
  synthesized `SDL_Event`s and asserting on the resulting state. The smallest deterministic
  interception points are `UpdateKeyboardState()`/`UpdateJoystick()` (replace the SDL poll
  with injected state) or the event pump (feed synthetic events).

### 1.5 File I/O `[verified-from-code]`: note only

- **Paths are centralized** through `SOURCES/DIRECTORIES.{H,CPP}`: `GetSavePath`,
  `GetCfgPath`, `GetResPath`, `GetVoicePath`, `GetMoviePath`, … all build on
  `SDL_GetPrefPath("Twinsen","LBA2")` (`DIRECTORIES.CPP:246`) and `SDL_GetCurrentDirectory`
  (`:221`), with case-folding discovery for case-sensitive filesystems (`:62–110`).
- **The RFC's "host services" already exist as functions:** `pref_path` →
  `SDL_GetPrefPath`; `base_path` → `SDL_GetCurrentDirectory`/`GetDefaultResDir`;
  `pick_folder` → `SDL_ShowOpenFolderDialog` (`RES_PICKER.CPP:185`, **async callback**);
  home default `SDL_GetUserFolder` (`RES_PICKER.CPP:175`).
- **I/O primitives** are wrapped: low-level `OpenRead/OpenWrite/Read/Write/Close`
  (`LIB386/SYSTEM/FILES.CPP`), mid-level `Load/LoadSize/Save` (`LIB386/SYSTEM/LOADSAVE.CPP`),
  HQR via `LoadMalloc_HQR`/`Load_HQR` (`LIB386/SYSTEM/HQRLOAD.CPP` → `OpenRead`). One
  exception uses raw stdio: the persisted game-dir state file in `RES_DISCOVERY.CPP`
  (`fopen/fgets/fprintf`), which is fine: one-shot, pre-init, located via `SDL_GetPrefPath`.

**Assessment:** Paths/IO are *already* abstracted enough. The PAL should expose the
host services (`pref_path`, `base_path`, `pick_folder`) because those are
the only live SDL calls in this group (`message_box` has no caller, dropped per §3.2); raw file IO does **not** need to enter the PAL (it is
portable C already). Do not over-engineer this.

### 1.6 Timing & threading `[verified-from-code]`

**The determinism linchpin already exists**: a harness-only fixed-dt virtual clock
(`LIB386/SYSTEM/TIMER.CPP:37–138`):

```c
// TIMER.CPP:90: arm a deterministic virtual clock; ManageTime() then reads
// FixedDtNow instead of SDL_GetTicks(). Runs that never call this are byte-for-byte
// unaffected (TIMER.H:34).
void Timer_EnableFixedDt(U32 dt_ms);
void Timer_FixedDtAdvance();   // +dt once per tick (TIMER.CPP:110)
void Timer_FixedDtPresent();   // accounts modal-inner-loop presents
void Timer_FixedDtPump();      // accounts non-presenting wait loops
S32  Timer_FixedDtActive();
// TIMER.CPP:144
void ManageTime(){ TimerSystemHR = FixedDtActive ? FixedDtNow : SDL_GetTicks(); ... }
```

It is wired to the CLI harness: `Control_Begin()` calls `Timer_EnableFixedDt()`
(`CONTROL.CPP:399`), `Control_TickHook()` calls `Timer_FixedDtAdvance()` (`CONTROL.CPP:674`).
`SDL_GetTicks` elsewhere is non-game-logic: mouse idle-hide (`MOUSE.CPP:41,74`), video
pacing (`PLAYACF.CPP:573–616`, with the loop's only `SDL_Delay(1)` at `:616`), perf
instrumentation (`PERFTRACE.CPP`), boot diagnostics (`PERSO.CPP:154–155`), dialog poll
(`RES_PICKER.CPP:190`).

**Threading is determinism-safe** `[verified-from-code]`: the only worker thread is the OGG
decode thread in `LIB386/AIL/SDL/STREAM.CPP:150` (`SDL_CreateThread`), coordinated with
lock-free `SDL_AtomicInt` and **joined** (`SDL_WaitThread`, `:156`) before its output is
read: audio I/O only, no game logic. `LOG.CPP` captures the main thread id to gate its
non-thread-safe console ring. No `std::thread`/`pthread`/game-logic workers exist. Loop
inversion and headless determinism are unaffected.

---

## 2. The RFC contract (starting point) `[from-RFC]`

Discussion #120 proposes `PlatformCallbacks` (verbatim), `extern PlatformCallbacks g_platform;`,
interface at `src/platform/platform.h`, backends in subdirs (`sdl/`, `headless/`,
`console/switch/`). Key stated properties: **input is a pull model**, **paletted is the
engine's native output**, **audio is not in the PAL**. Dependency rule: `engine → render.h,
platform.h, lib/`; `render → platform.h, lib/`; `platform → (nothing internal)`. Headless =
"all stubs, monotonic clock." Input injector:
`typedef struct { int scancode; uint64_t at_ms; } InputEvent; void headless_inject_input(InputEvent*, int);`

Full RFC struct (lifecycle / event-loop / time / display / input / host-services groups)
reproduced for mapping in §3. Note `Rect` is referenced (`present_frame(..., const Rect *dirty)`)
but **undefined** in the RFC.

---

## 3. Contract reconciliation

### 3.1 Call-site map (RFC callback → real code)

| RFC callback | Real site(s) | Verdict |
|---|---|---|
| `init` / `shutdown` | `InitWindow/EndWindow` `WINDOW.CPP:105/124`; `InitEvents/EndEvents` `EVENTS.CPP:40/52`; `CreateVideoSurface` `SDL.CPP` | **maps** (split across subsystems; PAL `init` fans out) |
| `poll_events` | `ManageEvents()` `EVENTS.CPP:65` | **maps cleanly** (single pump) |
| `should_quit` | `exit(0)` on `SDL_EVENT_QUIT` `WINDOW.CPP:454` | **GAP / resists**: no quit flag exists; must introduce one |
| `ticks_ms` | `ManageTime()`/`SDL_GetTicks` `TIMER.CPP:144`; fixed-dt `TIMER.CPP:90–138` | **maps** (determinism seam already present) |
| `delay_ms` | `SDL_Delay` `PLAYACF.CPP:616`, `RES_PICKER.CPP:190` | **maps** (minor, scattered) |
| `delay_events_ms` ("sleep, break on input") | wait-for-key loops (e.g. title/`WaitNoKey`-style) | **partial**: caller exists conceptually; exact site to be pinned in PR-3 `[assumed]` |
| `get_window_size` | `SDL_GetWindowSize` `WINDOW.CPP:51`; display mode `:227–231` | **maps** |
| `set_fullscreen` | `SetWindowFullscreen` `WINDOW.CPP:368` | **maps** |
| `set_vsync` | `Window_SetVSync` `WINDOW.CPP:414` | **maps** |
| `set_palette` | `SetVideoPalette` `SDL.CPP:169` | **maps** |
| `present_frame(indexed,w,h,dirty)` | `Lock/UnlockVideoSurface` `SDL.CPP:142/146` + `PresentFrame` `SDL.CPP:291` (reads global `Log` buffer); dirty via `DIRTYBOX` | **RESISTS**: see §3.4 |
| `key_down(scancode)` | `CheckKey()`/`TabKeys` `KEYBOARD.CPP:15–16`; `K_*≡SDL_SCANCODE_*` | **maps cleanly**: also covers gamepad via #58 virtual scancodes |
| `key_modifiers` | none direct (engine reads `K_CTRL/K_SHIFT/K_ALT` scancodes) | **DEAD** under the scancode model |
| `get_mouse(x,y,buttons)` | `MouseX/MouseY/Click` globals `MOUSE.CPP` | **maps** |
| `gamepad_count` | `JoystickIsPresent` `JOYSTICK.CPP` | **partial** (boolean today, not count) |
| `axis_value(pad,axis)` | `SDL_GetGamepadAxis` `JOYSTICK.CPP:321` (pre-digitized) | **DEAD/resists**: engine never consumes raw axes |
| `button_down(pad,btn)` | `SDL_GetGamepadButton` `JOYSTICK.CPP:304` (pre-digitized) | **DEAD/resists**: same; #58 routes via `TabKeys` |
| `pref_path` | `SDL_GetPrefPath` `DIRECTORIES.CPP:246` | **maps** (already a function) |
| `base_path` | `SDL_GetCurrentDirectory` `DIRECTORIES.CPP:221` | **maps** |
| `pick_folder` | `SDL_ShowOpenFolderDialog` `RES_PICKER.CPP:185` (async) | **resists**: async callback vs synchronous `int` return |
| `message_box` | no caller found in audit | **DEAD** (no current consumer) `[assumed]` |

### 3.2 Dead callbacks (in RFC, no real caller)

`key_modifiers` (scancode model makes it redundant), `axis_value` + `button_down` (engine
sees only digitized virtual scancodes; adopting raw-axis pull would be a **behavior
change**, out of scope), `message_box` (no consumer found). Keep `axis_value`/`button_down`
out of the v1 struct unless/until a feature needs raw analog; document why.

### 3.3 Gaps (real call sites with no RFC callback)

- **Text entry:** `GetAscii()` `KEYBOARD.CPP:69` (save-name input): needs a text-input seam.
- **Existing function-pointer hooks already in the present/event path** (the PAL should
  subsume or preserve these rather than reinvent): `s_eventFilter` `EVENTS.CPP:37`;
  `s_prePresent`/`s_overlayPrePresent`/`s_presentBegin`/`s_presentEnd` `SDL.CPP:33–35`
  (overlay + perf + screenshot hooks); `ApplyVirtualKeys` weak hook (touch re-injection).
- **Focus → audio pause:** `AppActive` set in `HandleEventsWindow` `WINDOW.CPP:432–446`.
- **Cursor hide:** `SDL_HideCursor` `WINDOW.CPP:32`.
- **Runtime resolution switch:** `SDL_SetWindowSize` `RES_SWITCH.CPP:217`.
- **System probes / libc shims:** `SDL_GetSystemRAM`, `SDL_GetNumLogicalCPUCores`,
  `SDL_GetPlatform`, `SDL_malloc/strlen/...` (Groups C). These are one-shot host queries,
  not per-frame; they belong in a `host`/`sys` section, not the frame loop.
- **The CLI stepping harness** (`Control_*`): not in the RFC, but it is the gate-0 enabler
  and the natural home of `Step_OneFrame()` + the input injector.

### 3.4 Sites that resist the function-pointer model

(The numbered items below are referenced elsewhere as §3.4.1–§3.4.5.)

1. **`present_frame()` impedance mismatch.** The RFC models present as a push of an indexed
   buffer + a single dirty `Rect`. Reality: the engine renders into a **global** paletted
   buffer (`Log`), and present is **pulled** when the lock count hits zero
   (`UnlockVideoSurface`, `SDL.CPP:146–157`, gated by `frameDirty` `:225`), with dirty
   regions tracked by the engine's own **dirty-box subsystem** (`DIRTYBOX`), not a single
   rect. The desktop vs Android present paths differ (lock-texture vs staging-buffer).
   **Reconciliation:** model present as `{ begin_frame/lock, (engine writes paletted
   buffer), end_frame/present }` plus `set_palette`, and let `platform_sdl` keep the
   dirty-box/lock-count/desktop-vs-Android details private. Do **not** force the RFC's
   `present_frame(indexed,w,h,dirty)` signature; adapt it to the lock/present pair.
   **Critically, the lock takes no drawable-pixels out-param in v1.** The engine writes the
   global `Log` buffer all frame via the existing SVGA primitives; `PresentFrame`
   (`SDL.CPP:291`) reads `Log` at present time, and the `AcquirePresentTarget` pixel
   acquisition is platform-private. A `void **pixels` handoff would describe the *console*
   model (engine renders directly into the platform framebuffer), a real behavior change
   scoped out of v1. Reintroduce the render-target handoff only when a console backend needs
   it; otherwise the first console author wires up `frame_begin` to hand back a surface the
   engine never wrote to.

2. **Raw-axis input pull** (`axis_value`/`button_down`): see §3.2; the engine's contract is
   "is virtual-scancode N down?", established by #58. Generalize **that**, don't fight it.

3. **`pick_folder` async.** `SDL_ShowOpenFolderDialog` is callback-based; a synchronous
   `int pick_folder(char*,int)` would require pumping events until the callback fires. Keep
   the async shape inside `platform_sdl` and expose a small state-machine or
   "request/poll-result" pair, or accept a blocking wrapper.

4. **Quit.** `should_quit()` needs a real quit flag to read; today quit is `exit(0)`. PR-5
   introduces a `g_quitRequested` flag set in `HandleEventsWindow`.

5. **Key vocabulary = SDL scancodes** (`KEYBOARD_KEYS.H`). For strict behavior preservation,
   keep `K_*≡SDL_SCANCODE_*` and **carve the header out of the guard**; a true decoupling
   (engine-owned scancode enum + translation table at the SDL boundary) is a *separate*,
   behavior-neutral-but-large change; flag, don't bundle.

### 3.5 Refined contract (proposed `g_platform`)

A pragmatic, scancode-centric struct that matches the code (rationale per change). Header
lives at `LIB386/PLATFORM/PLATFORM.H`; instance `extern PlatformInterface g_platform;`.

**Filename convention** `[verified-from-code]`: the engine tree is **UPPERCASE** `.CPP`/`.H`
(510 first-party files; the only lowercase ones are vendored single-headers), and every
new-infrastructure file with no ASM original (`CONTROL.CPP`, `LOG.CPP`, `PERFTRACE.CPP`,
`RES_PICKER.CPP`, `TOUCH_INPUT.CPP`) already follows it. New PAL files therefore use
`PLATFORM.H`, `PLATFORM_SDL.CPP`, `PLATFORM_HEADLESS.CPP`, **not** the RFC's lowercase
`src/platform/platform.h` (that lowercasing is part of Phase 0, which this plan drops). The
*variable* `g_platform` stays lowercase to match the existing `g_main_thread` idiom
(`LOG.CPP:75`). If Phase 0 ever runs, these files normalise with the rest.

```c
typedef struct PlatformInterface {
    /* lifecycle: fans out to InitWindow/InitEvents/CreateVideoSurface today */
    bool (*init)(const PlatformConfig *cfg);
    void (*shutdown)(void);

    /* frame loop */
    void (*poll_events)(void);            /* == ManageEvents()                       */
    bool (*should_quit)(void);            /* NEW flag; replaces hardcoded exit(0)    */

    /* time: fixed-dt seam already exists behind this */
    uint64_t (*ticks_ms)(void);           /* == ManageTime() clock source            */
    void     (*delay_ms)(uint32_t ms);
    void     (*delay_until_input_ms)(uint32_t ms);  /* RFC delay_events_ms; pin caller */

    /* display: paletted native; the engine writes the global indexed buffer (Log) all
       frame via existing SVGA primitives. frame_begin/end are the lock-count seam
       (LockVideoSurface/UnlockVideoSurface), NOT a pixel handoff, no drawable-pixels
       out-param in v1 (see §3.4.1). */
    void (*get_window_size)(int *w, int *h);
    void (*set_fullscreen)(bool fs);
    void (*set_vsync)(bool on);
    void (*set_window_size)(int w, int h);          /* RES_SWITCH runtime resolution   */
    void (*set_palette)(const uint8_t *rgb_256);
    void (*frame_begin)(void);                       /* == LockVideoSurface (lock count)*/
    void (*frame_end)(void);                          /* == UnlockVideoSurface: Log→target copy + present */
    /* dirty-box, desktop/Android split, ARGB8888 conversion, and the AcquirePresentTarget
       drawable-pixels acquisition all stay private to platform_sdl */

    /* input: PULL via virtual scancodes (generalizes PR #58); raw axes intentionally absent */
    bool (*key_down)(int scancode);       /* keyboard AND gamepad virtual scancodes  */
    void (*get_mouse)(int *x, int *y, uint32_t *buttons);
    int  (*text_input)(void);             /* == GetAscii(); save-name entry          */
    int  (*gamepad_count)(void);          /* from JoystickIsPresent (widen to count) */

    /* host services: already functions today */
    const char *(*pref_path)(void);       /* SDL_GetPrefPath                          */
    const char *(*base_path)(void);       /* SDL_GetCurrentDirectory                  */
    int          (*pick_folder)(char *out, int outsz); /* wraps async SDL dialog      */
} PlatformInterface;
extern PlatformInterface g_platform;
```

**Changes from the RFC and why:**
- **Dropped** `key_modifiers`, `axis_value`, `button_down`, **and `message_box`**: dead or
  unconsumed (§3.2); carrying a member with no caller is the exact inconsistency this struct
  cleans elsewhere, and `axis_value`/`button_down` would also invite a behavior change.
- **`present_frame` → `frame_begin`/`frame_end` + `set_palette`, with no pixel out-param**:
  matches the real lock/commit + global-`Log`-buffer + dirty-box reality (§3.4.1); hides
  desktop/Android split. The RFC's drawable-pixels handoff (`void **pixels`) describes the
  *console* render-into-platform-framebuffer model, a behavior change deferred out of v1.
- **Added** `should_quit` (real flag), `text_input` (`GetAscii`), `set_window_size`
  (`RES_SWITCH`), `delay_until_input_ms` (RFC `delay_events_ms`, caller TBD).
- **`gamepad_count`** widened from today's boolean `JoystickIsPresent`.
- **Audio absent by design.** AIL owns it (§1.3).
- **Not in the struct (kept as a separate `host`/`sys` shim or left as-is):** sysinfo probes
  (`SDL_GetSystemRAM`/CPU/platform), libc shims (`SDL_malloc/strlen`), logging, perf: these
  are one-shot or diagnostic, not frame-loop concerns; forcing them into `g_platform` adds
  churn without portability benefit. They get their own tiny `platform_sys.*` only if the
  guard ratchet demands it.

The struct stays a C header with `extern "C"` linkage and stdint types, so it remains
shape-compatible with the RFC's "shared `adeline-common`" aspiration.

### 3.6 "No moves" vs. true containment: decide before PR-1 `[design]`

In-place forwarding gives the engine a `g_platform` seam, but the SDL *code* stays in
`LIB386/SYSTEM`/`LIB386/SVGA`. Two consequences: (a) `lba2_platform` links *into* `system`/
`svg` (`platform → system`), inverting the RFC's `platform → nothing internal` rule; (b) the
PR-7 guard cannot honestly flip to "error / no `SDL_` outside `PLATFORM_*`", because the
Group A files still hold most of the SDL surface. Pick one resolution:

- **Option A (recommended): targeted move *and split*, not wholesale relocation.** This
  deliberately **reopens the "nothing moves" stance**, on purpose, for honest `platform →
  nothing internal` and a real guard. But the ~8 files are *not* pure-SDL, so don't drag the
  game tree with them. Two dispositions:
  - **Clean move** (pure platform glue → `PLATFORM_*.CPP`): `WINDOW.CPP` → `PLATFORM_WINDOW.CPP`;
    `SVGA/SDL.CPP` → `PLATFORM_VIDEO_SDL.CPP`; `EVENTS.CPP` → `PLATFORM_EVENTS.CPP`.
  - **Split** (move only the SDL call; leave engine state/logic where it lives): `TIMER.CPP`
    keeps the fixed-dt clock + `TimerRefHR`; `KEYBOARD.CPP` keeps `TabKeys[]`/`CheckKey`/
    `GetInput`; `MOUSE.CPP` keeps its state globals; `JOYSTICK.CPP`/`TOUCH_INPUT.CPP` (in
    `SOURCES/`, the **game tree**) keep `ButtonMap`/digitization/#58 bindings. Lift only the SDL
    poll (`SDL_GetTicks`, `SDL_GetKeyboardState`, `SDL_GetMouseState`, `SDL_GetGamepad*`) into
    the platform backend. **Move what's SDL; don't relocate the game tree.**

  Add the move SHA to `.git-blame-ignore-revs`. (The split-side files, with their SDL poll
  lifted out, no longer trip the guard and need no allow-list entry.) Provenance per moved or
  split file, and the `lba2-classic` lineage it preserves, is tabled in §5.2.
- **Option B: strict no-moves, downgrade the guard.** Allow-list the Group A files
  permanently; the guard then means "no *new* SDL creeps into game/engine code" (still useful
  regression-prevention), and the doc must stop claiming physical SDL containment or RFC-style
  layering until a later move.

Either is defensible. What is **not** defensible is wanting "no moves" *and* a strong guard
*and* clean layering all at once; those can't all hold. This decision changes the
CMake source lists and the guard allow-list, so it gates PR-1.

**Reconciliation with `ENGINE_GAME_SEAM.md` + why portability forces A.** That doc already calls
`SYSTEM`/`SVGA` + the community port layer "the PAL" (`ENGINE_GAME_SEAM.md:266`) and treats the
`engine·platform` split as acceptable *heritage*, grounding any boundary move in "strangler-fig
boundaries that pay off twice" (`:270–287`), i.e. don't move files for dependency-purity alone.
**Option A clears that bar on portability grounds, not purity:** the second payoff is a build
that links no SDL at all, the precondition for the non-SDL hosts in §5.1 (raylib; GameCube/Wii
via libogc). So if portability to a non-SDL host is a real goal, Option A is **required**, not
preferred; Option B yields only a cleaner desktop-SDL build, never an SDL-free one. (§3.7's
compile/link selection is the matching half: pick one host backend at build time, never two.)

### 3.7 Contract shape × selection time: two axes, not one, decide before PR-1 `[design]`

These are independent choices, easy to conflate into a false binary:
- **Contract shape:** a `g_platform` *struct of function pointers* (self-documenting; one
  artifact *is* the whole PAL contract; makes the console/libretro swap legible) vs *direct
  calls* (no contract surface).
- **Selection time:** which backend populates the contract, chosen at **compile/link** (one
  backend built, like AIL's `SOUND_BACKEND` at `LIB386/AIL/CMakeLists.txt:3`) vs **runtime**
  (multiple backends linked, swap `g_platform` live).

**Recommendation: keep the struct as the contract artifact, and select the backend at
compile/link** via a CMake `PLATFORM_BACKEND` option exactly like `SOUND_BACKEND`. That buys
AIL-idiomatic, zero-switch-machinery selection *and* one-struct-is-the-whole-contract clarity.
The only cost is a function-pointer hop on `key_down`/present, unmeasurable noise; do **not**
collapse to direct calls to save it, or you throw away the contract surface that is half the
point of the PAL. Live runtime swapping (multiple backends linked) is a Phase-3 extra
(SDL_GPU→software fallback) the struct already leaves the door open for, not needed in v1.
This shapes the scaffold, so it gates PR-1.

---

## 4. Extraction plan (PR-sequenced, strictly behavior-preserving)

Every PR: `platform_sdl` wraps today's behavior exactly; zero gameplay change; verified
against existing CI (`host_quick` tests, the fixed-dt demo determinism tests, and the
screenshot/present harness) before merge. In-place only: `LIB386/PLATFORM/`, **no moves**.

**PR-0 (GATE-0): invert the loop to `Step_OneFrame()`.** `[verified-from-code]`
Hoist the `while (TRUE)` body of `MainLoop()` (`PERSO.CPP:478–~1958`) into
`void Step_OneFrame(void)`; convert the exits in §1.1 to a `static` state flag
(`exitCode`/`loopState`); `MainLoop()` becomes `while (exitCode<0) Step_OneFrame();`. May
still call SDL indirectly; this PR proves *structure*, not isolation. **Verify:** fixed-dt
demo determinism byte-for-byte identical **and** the exit/restart paths the demo never
exercises: save→load round-trip (the `goto restartloop` reload at `:552`/`:1010`),
open-and-exit a modal menu (the `break` cluster `:1102–1571`), and ESC/quit (the early
`return`s). Those four conversion classes are the likely-wrong ones, and PR-0 is the
irreversible lead merge, so its gate must hit them, not just the happy-path tick loop;
`host_quick` green. *Lead PR: loop-structure de-risked by the existing `Control_*` harness;
the exit/restart conversions are the real risk.*

**PR-1: Scaffold.** Add `LIB386/PLATFORM/PLATFORM.H` (the §3.5 contract), `g_platform`
definition, and `PLATFORM_SDL.CPP` that **initially just forwards** to existing functions
(`ManageEvents`, `SetVideoPalette`, `Window_SetVSync`, …). Add the `lba2_platform` CMake
target with an **explicit source list** under `LIB386/PLATFORM/`. **Settle §3.6 (move vs
no-move) and §3.7 (runtime vs compile-time) first; both shape this scaffold and its source
list.** No call sites change yet; target compiles, links, and is unused. Add the **grep
guard script in warn-only mode** (CI annotation, never fails) with the allow-list from §1.2.
**Verify:** build all presets; tests unchanged.

**PR-2: Input (generalize PR #58).** Route input through `g_platform.key_down` /
`get_mouse` / `text_input`. Move `SDL_GetKeyboardState`/`SDL_GetGamepad*` behind
`platform_sdl`; the `TabKeys[]` data seam and #58's virtual-scancode digitization stay
intact (engine still asks "is scancode N down?"). Decide the `K_*≡SDL_SCANCODE_*` carve-out
(keep + allow-list the header for v1). **Verify:** `tests/input_device`, gamepad still maps,
demo replay identical, save-name text entry works; record the `KEYBOARD.CPP` provenance row (§5.2).

**PR-3: Ticks.** Route `ticks_ms`/`delay_ms`/`delay_until_input_ms` through `g_platform`;
`platform_sdl.ticks_ms` wraps the `ManageTime()` clock source; the fixed-dt seam
(`TIMER.CPP:90–138`) plugs straight into the headless backend later. Pin the
`delay_until_input_ms` caller. **Verify:** fixed-dt determinism tests unchanged; FPS counter
sane; record the `TIMER.CPP` provenance row (§5.2).

**PR-4: Video / present.** Move `sdlWindow/sdlRenderer/sdlTexture` ownership and the
present path (`frame_begin`/`frame_end`/`set_palette`/`get_window_size`/`set_fullscreen`/
`set_vsync`/`set_window_size`) into `platform_sdl`, keeping dirty-box, lock-count,
ARGB8888 conversion, letterboxing, and desktop-vs-Android split private. Preserve the
existing `s_prePresent`/overlay/screenshot hooks. **Verify:** the screenshot/present harness
produces identical frames; runtime resolution switch (`RES_SWITCH`) still works. ⚠ The
screenshot harness runs on **desktop CI only**, so the Android staging-buffer present path
(MTE-sensitive, landed in the now-merged PR [#261](https://github.com/LBALab/lba2-classic-community/pull/261)) is refactored-but-unverified by default;
add an Android present-path check or on-device verification for this PR specifically; record the
present-path provenance row (§5.2).

**PR-5: Quit + window events + host services.** Replace `exit(0)` (`WINDOW.CPP:454`) with a
`g_quitRequested` flag behind `should_quit()`; route focus/cursor/quit window events and the
`pref_path`/`base_path`/`pick_folder` host services through `g_platform`. **Verify:** clean
shutdown path; folder picker first-launch flow; focus-loss still pauses audio.

**PR-6: `platform_headless` + injector + first integration test (loop-inversion proof).**
Add `LIB386/PLATFORM/PLATFORM_HEADLESS.CPP`: all stubs + monotonic/fixed-dt clock +
`headless_inject_input(InputEvent{scancode,at_ms}*, int)` feeding the same `TabKeys[]`/
virtual-scancode seam used by #58 and `tests/input_device`. Build a headless target that
sets `g_platform=headless`, `SOUND_BACKEND=null`, injects scripted input, drives
`Step_OneFrame()` for N ticks, and asserts engine state. This is the *running* headless
stepper, landing after real seams exist (per the §0 sequencing decision). **Verify:** new
integration test in CI (CTest label `host_quick` or a new `integration` label).

**PR-7: Guard ratchet to error.** Flip the grep guard from warn to **error**: no `SDL_` /
`#include <SDL` outside the allow-list (`LIB386/PLATFORM/PLATFORM_SDL*.CPP`,
`LIB386/AIL/SDL/*`, and the explicitly enumerated Group C/D tail + carved-out
`KEYBOARD_KEYS.H`/`KEYBWIN.H`). **This PR's reach depends on the §3.6 decision:** under
Option B (no move), the Group A files (`WINDOW.CPP`, `EVENTS.CPP`, `SDL.CPP`, `TIMER.CPP`,
`KEYBOARD.CPP`, `MOUSE.CPP`, `JOYSTICK.CPP`) must *also* be allow-listed, which downgrades
the guard to "no *new* SDL in game/engine code" rather than "SDL contained in `PLATFORM_*`".
Under Option A (targeted move) those files *are* `PLATFORM_*.CPP` and the strong guard holds.
State which in the PR. **Verify:** CI fails on a planted violation; passes clean.

**Guard ratchet summary:** warn-only from PR-1 → error at PR-7. The allow-list is the honest
record of "what still touches SDL on purpose" (audio backend + diagnostics + sysinfo shims +
the SDL-scancode header). Shrinking that list further (e.g. engine-owned scancodes, a
`platform_sys` for sysinfo) is follow-on work, explicitly out of this plan's behavior-
preserving scope.

---

## 5. Non-goals & where other backends attach

- **No Phase 0 / full case normalization.** Not the RFC's ~600-file restructure. (May include
  the single targeted ~8-file move of §3.6 Option A, a separate, explicit decision, *not* Phase 0.)
- **Modal inner loops are not inverted.** Gate-0 inverts the main loop; the blocking modal/wait
  loops remain, fine for the headless stepper, owed as inversion debt before pull-driven
  frontends (see §5). Scoped out.
- **No functional changes.** Pure refactor. If a behavior change *seems* required (e.g. raw
  analog axes, engine-owned scancodes, graceful-quit semantics beyond the flag), **flag it**
  (§3.4); do not make it.
- **No console/handheld backends built here**, one paragraph each on attachment:
  - **libretro:** implements `PlatformInterface`; `retro_run()` calls `Step_OneFrame()` once.
    ⚠ **Gate-0 inverts the *main* loop only.** The modal/wait inner loops (menus, `GamePaused`,
    `RES_SWITCH` dialogs, the very loops `Timer_FixedDtPresent`/`Pump` exist to account for)
    still **block**: under libretro a modal menu means `retro_run()` never returns and the
    frontend reads the core as hung. Harmless for the headless stepper this plan delivers (the
    harness drives those inner loops), but real **inversion debt** owed before libretro:
    converting each modal inner loop to a resumable state machine. Logged here, scoped out.
    Video via `frame_begin`/`frame_end`; audio via an AIL backend or libretro audio batch.
  - **Switch (devkitPro):** its SDL3 port can reuse `platform_sdl` largely as-is; only
    fullscreen/window assumptions and the picker need console-appropriate stubs.
  - **PSP / GameCube / Analogue Pocket:** native `PLATFORM_*.CPP` implementing the same
    struct; paletted output (`frame_begin`/`set_palette`) maps to each framebuffer; no SDL.
    The KallistiOS Dreamcast precedent (software renderer → blit) shows the paletted-native
    path already ports without touching the engine.
  - **raylib (peer host lib, the truest SDL swap):** `PLATFORM_RAYLIB.CPP` maps near 1:1:
    `WindowShouldClose()`→`should_quit`, `PollInputEvents()`→`poll_events`, `frame_end` does
    `Log`→RGBA→`UpdateTexture`→`DrawTexture`. The one friction: raylib's `KeyboardKey` enum is
    not SDL scancodes, so `key_down()` must reverse-map, the clearest case for pulling the
    engine-owned-scancode work (§3.4.5) forward rather than deferring it.

### 5.1 Portability validation: does the seam actually permit non-SDL hosts? `[design]`

The reason any of this is feasible: rendering is a **software-rasterised 8-bit paletted
framebuffer** (the global `Log` buffer) + a 256-entry palette, no GPU/shader API required, so
every backend only needs "present this indexed buffer + palette." The three targets below are
*different kinds* of thing and stress different parts of the contract:

- **raylib:** a *peer host library*, the cleanest swap. Viable as `PLATFORM_RAYLIB.CPP` + an
  `ail_raylib` AIL backend. Only wart: the SDL-scancode input vocabulary (§3.4.5).
- **libretro** is *not* a host lib but an **inversion-of-control frontend**: the frontend owns
  the loop and `retro_run()` == `Step_OneFrame()` (the plan is built for this). **But**
  shippable also needs the modal-loop inversion debt cleared (a blocking modal hangs the
  frontend, §5) *and* a libretro AIL backend (audio batch). PAL alone is necessary, not
  sufficient.
- **"Dolphin" is an *emulator*, not a host lib.** There is no `platform_dolphin`. The target is
  **GameCube/Wii native** (devkitPPC + libogc + GX blit + an AESND AIL backend), which Dolphin
  then runs. Feasible precisely because of the paletted software framebuffer; real blockers are
  big-endian PowerPC and the software renderer's non-portable FP determinism, both already
  tracked in `docs/PLATFORM.md`, neither a PAL concern.

What the PAL does **not** provide for any of them: a per-host **AIL backend** (audio is a sibling
contract, §1.3), the **modal-loop inversion** (libretro), and a **host-neutral input vocabulary**
(every non-SDL backend reverse-maps into SDL scancodes until §3.4.5 lands). And critically, **all
three require §3.6 Option A** to produce a build with no SDL linked; under Option B, SDL stays
linked unconditionally, which is fine for desktop but a dead end for a GameCube/Wii target that
must not link SDL at all.

### 5.2 Traceability to `lba2-classic` `[verified-from-code]` `[design]`

A preservation guarantee, since this fork exists to keep the 1997 Adeline lineage legible.
**The PAL is confined to the community SDL3 layer; the upstream-traceable core (ASM-equivalence
pairs, the Life/Track script VM, the file-format parsers) is never moved or modified.** Two
kinds of lineage, and the PAL touches only one:

- **Line-level / ASM-equivalence lineage** (engine math, the SVGA rasteriser fillers, format
  parsers, the script VM) traces 1:1 to the Adeline source and is pinned by
  `ASM_TO_CPP_REFERENCE.md` + the Docker equivalence tests. The PAL touches **none** of it: it
  moves `LIB386/SVGA/SDL.CPP` (the SDL3 present backend), not the rasteriser `.ASM`
  (`SCALEBOX.ASM`, `COPYMASK.ASM`, `BLITBOXF.ASM`, …), which stay put with their tests.
- **Subsystem "SDL3 replacement of upstream's DOS X" lineage** (the platform/IO layer) is what
  the PAL reorganizes. Every file it clean-*moves* is community SDL3 code with no upstream
  ancestry (no `.ASM` pair); the upstream DOS originals it replaced (`KEYB.ASM`,
  `MOUSEDAT.ASM`) are dormant and untouched.

Provenance per PAL-touched file (in the `LBA1_PORTING_SURFACE.md` verdict-table style):

| File(s) | `lba2-classic` origin | PAL action (§3.6) | Lineage preserved by |
|---|---|---|---|
| `SVGA/SDL.CPP`, `WINDOW.CPP`, `EVENTS.CPP` | community SDL3 (replaces DOS VESA/event/timer); no ASM pair | clean move → `PLATFORM_*.CPP` | `.git-blame-ignore-revs` (no upstream version to diverge from) |
| `TIMER.CPP`, `KEYBOARD.CPP`, `MOUSE.CPP` | community SDL3 rewrite of DOS originals (`KEYB.ASM`/`MOUSEDAT.ASM` dormant) | **split**: lift SDL poll, keep clock/`TabKeys[]`/state in place | engine logic + comments stay in the original file |
| `JOYSTICK.CPP`, `TOUCH_INPUT.CPP` | community (SDL3 gamepad / Android touch) | stay in `SOURCES/`; lift SDL poll only | unchanged path |
| SVGA rasteriser `.ASM`, script VM (`GERELIFE`/`GERETRAK`), format parsers | **verbatim Adeline lineage** | **untouched** | ASM-equivalence tests, unchanged |

Mechanisms (all already in place): `.git-blame-ignore-revs` exists today and gains the move
SHA (§3.6), so `git blame` survives the rename; the ASM-equivalence suite is undisturbed (it
pairs the computational modules, none of which move); the preservation rules (French comments,
ASCII banners per `CODESTYLE.md`) ride along in the split/moved files.

**Apply at split time, not retroactively.** This table is a commitment to *verify when the line
is drawn*, not an after-the-fact label. PR-2 (input), PR-3 (ticks), and PR-4 (video) are where
`KEYBOARD.CPP` / `TIMER.CPP` / the present path actually get cut into SDL-glue vs in-place engine
state, so the ASM-traceable-vs-SDL-glue boundary is decided in those PRs. Fill in or confirm each
file's row as part of that PR's review, so the provenance records the real cut.

Relation to `LBA1_PORTING_SURFACE.md`: that doc is a *different axis* (hosting LBA1 *content* on
this engine), but it already names the LBA1/LBA2 divergence as concentrated in "I/O wrappers,
the media stack, and rendering capability", precisely the platform/IO layer the PAL isolates. So
the PAL *helps* both stories: quarantining the divergent layer behind one seam leaves the shared
Adeline core (the part traceable to `lba2-classic`) cleaner and clearly delineated.

---

## 6. Open decisions for go/no-go

1. **Sequencing** (§0): confirm "prove inversion first (PR-0, may still call SDL), land
   running headless after partial migration (PR-6)." *Recommended.*
2. **Scancode vocabulary** (§3.4.5): v1 keeps `K_*≡SDL_SCANCODE_*` + header carve-out
   (behavior-preserving). Engine-owned scancodes = separate later effort. Confirm.
3. **Audio** (§1.3): confirm AIL stays a *sibling* contract (no "AIL → PAL sink"); headless
   uses `SOUND_BACKEND=null`. *Recommended.*
4. **Dead RFC callbacks** (§3.2): confirm dropping `key_modifiers`/`axis_value`/
   `button_down`/`message_box` from v1.
5. **`present_frame` reshape** (§3.4.1): confirm `frame_begin`/`frame_end`+`set_palette`
   over the RFC's `present_frame(indexed,w,h,dirty)`.
6. **Guard allow-list** (§4/PR-7): confirm the Group C/D tail (sysinfo/libc/log/perf/console)
   may remain SDL-touching behind the allow-list rather than being migrated in this plan.
7. **Doc home:** extend the existing `docs/PLATFORM.md` (status legend already
   ✓contained/⚠partial/✗exposed) as PRs land, rather than a parallel doc. Confirm.
8. **Containment vs. no-moves** (§3.6, **gates PR-1**): pick **Option A** (targeted move
   *and split*: clean-move the pure-SDL glue, lift only the SDL poll from the timer/input
   files, leave engine state and the `SOURCES/` game tree in place → strong guard, honest
   layering) or **Option B** (strict no-moves → regression-only guard). Option A reopens the
   earlier "no moves" call on purpose. **For any non-SDL host target (§5.1) Option A is
   *required*, not preferred**: it clears `ENGINE_GAME_SEAM.md`'s "pays off twice" bar on
   portability grounds (§3.6). *A recommended.*
9. **Backend dispatch** (§3.7, **gates PR-1**): confirm **keep the `g_platform` struct as the
   contract *and* select the backend at compile/link** (CMake `PLATFORM_BACKEND`, like
   `SOUND_BACKEND`). This is not struct-vs-direct-calls; keep the struct, the pointer hop is
   noise. *Recommended.*
10. **Filename casing** (§3.5): new PAL files are UPPERCASE (`PLATFORM_SDL.CPP`), matching the
    510-file tree, not the RFC's lowercase. *Recommended; confirm.*
11. **PR-0 verification scope** (§4): confirm PR-0's gate covers save/load-restart, modal
    open/exit, and quit paths, not just demo determinism, given it is the irreversible lead.

---

*Prepared read-only. No source, build, or layout changes were made. Awaiting go/no-go.*
