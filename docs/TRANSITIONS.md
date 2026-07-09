# Scene and FMV transitions

How the engine moves between scenes (cubes) and in/out of full-screen videos: the
fade-out, the load, and the fade-in reveal. If you are touching anything that
loads a scene, plays a cutscene, or changes the palette, read this first.

## The core principle: black brackets the load

A transition must **hide the load on both sides**. The screen fades to black, the
old scene is torn down and the new one is loaded *while the screen is black*, and
then the screen fades back up on the fully-composited new scene. Nothing that
changes on screen (actors disappearing, geometry swapping, a scene popping in)
should ever be visible at a lit palette.

The reveal is a **palette fade**, not a content fade. You cannot fade content that
is not on screen. So the sequence is always:

1. compose the new scene into the `Log` framebuffer,
2. present it while the hardware palette is **black** (the viewer sees black),
3. ramp the palette up from black to the scene's palette (the reveal).

The key consequence: **the palette must already be black when the new scene is
first presented.** If it is not, the scene flashes bright for one or more frames
before the fade-in starts. That flash is the #404 bug.

## The two clocks (see docs/TIMING.md)

Fades loop on `TimerSystemHR`, the managed game clock (`SDL_GetTicks` in real time,
`FixedDtNow` under `--fixed-dt`). `FADE_DELAY` is 200 ms (`SOURCES/AMBIANCE.CPP`).
The ramp is **time-based**: each iteration reads the clock and computes how far
through the 200 ms it is, so the fade lasts ~200 ms regardless of frame rate. The
step *count* scales with fps; the *duration* does not.

Timing a fade on raw wall-clock instead of the managed clock is a regression class
(#353 / #374): `SDL_GetTicks` counts the synchronous cube load, so the fade's
baseline goes stale and it collapses to a single step ("actors pop in at full
brightness"). Fades must ramp on `TimerSystemHR`.

## State flags

| Flag | Meaning |
| --- | --- |
| `FlagBlackPal` | The hardware palette is black. `FadeToBlack*` early-returns when it is already set. Set TRUE by `SetBlackPal` and by the end of any fade-to-black; cleared by the end of any fade-to-pal. |
| `FlagFade` | "A scene fade-in is pending." Set by a transition; **consumed at the tail of `AffScene`**, which calls `FadeToPalAndSamples` to reveal. This is the reveal trigger. |
| `FlagPal` | "Sync the palette now, no fade." Consumed just before `FlagFade` in `AffScene`. |
| `FlagLoadGame` | Loading a save. The fade-out at load time is skipped (the load is silent); `AffScene` syncs the palette directly instead of fading. |
| `FlagChgCube` | A genuine cube transition (drives the per-scene autosave and the timer save); not set by console warps or menu loads. |
| `FirstTime` / `AFF_*` | The `AffScene` flip mode. `AFF_OBJETS_FLIP`=0, `AFF_ALL_FLIP`=1, `AFF_OBJETS_NO_FLIP`=2, `AFF_ALL_NO_FLIP`=3 (`COMMON.H`). A transition sets `FirstTime = AFF_ALL_FLIP`. |
| `PtrPal` / `PtrPalNormal` / `PtrPalBlack` | The current / normal-scene / all-black palettes. `PtrPal` is a pointer that `ChoicePalette` re-points; it does **not** itself sync the hardware. |

## The fade primitives (`SOURCES/AMBIANCE.CPP`)

All ramp on `TimerSystemHR` via `Timer_FixedDtPump()` + `ManageTime()`, and drive
`FadePal`, which builds one interpolated palette (`RegleTrois`) and pushes it with
`PaletteSync` + `BoxBlit` (i.e. it presents one frame per step). `BoxBlit` is
present-only, deliberately not `BoxUpdate` -- see the object-layer-wipe class below.

| Function | Does |
| --- | --- |
| `FadeToBlack(pal)` | Ramp `pal` down to black. No-op if `FlagBlackPal`. Sets `FlagBlackPal`. |
| `FadeToPal(pal)` | Ramp black up to `pal`. Clears `FlagBlackPal`. |
| `FadeToBlackAndSamples(pal)` | `FadeToBlack` + ramp SFX volume down + `PauseSamples`. The scene-transition / FMV variant. |
| `FadeToPalAndSamples(pal)` | `FadeToPal` + resume/ramp SFX volume up. The reveal variant `AffScene` calls for `FlagFade`. |
| `FadePal(r,g,b,pal,scale)` | One frame: interpolate every colour between `(r,g,b)` and `pal` at `scale/FADE_DELAY`, `PaletteSync`, then `BoxBlit` (present only). |
| `SetBlackPal()` | Force the hardware palette to all-black immediately (no ramp) and set `FlagBlackPal`. |
| `FadePalToPal(a,b)` | Cross-fade palette `a` to `b` (used by `FadeToPalIndex` / the `LM_FADE_TO_PAL` script opcode for a lit-to-lit change). Presents each step via `BoxBlit`, like `FadePal` (#418). |
| `WhiteFade` / `FadeWhiteToPal` | White-flash variants (special effects, not the normal transition path). |
| `ChoicePalette()` | Load the cube's `.XPL` palette and re-point `PtrPal` to it. **Does not sync the hardware palette** and does not set `FlagPal`; the reveal that follows is expected to sync it. |

The `AndSamples` variants deliberately time the *visual* fade on `TimerSystemHR`
and only use the audio sample-fade return value to ramp SFX volume, so audio state
can never break the visual fade (part of the #353 fix).

## Buffers and the present path

- `Log` -- the composited framebuffer. This is what a fade operates on; the scene is
  drawn here, then the palette ramp reveals it.
- `Screen` / `ScreenAux` -- clean backgrounds. `BackupScreen(flag)` / `RestoreScreen()`
  (`SOURCES/INVENT.CPP`) snapshot and restore around modals; `RestoreScreen` re-renders
  via `AffScene`, so it is game-context only.
- `Phys` / `paletteLUT` -- the SDL present target and the active hardware palette
  (`LIB386/SVGA/SDL.CPP`).
- `BoxBlit()` -> `PresentFrame()` -- one present. Every fade step presents (via
  `FadePal` -> `BoxBlit` -> `PresentFrame`), so a 200 ms fade is many presents.

## Scene transition orchestration (`ChangeCube`)

Entry: the main loop (`MainLoop`, `SOURCES/PERSO.CPP`) sees `NewCube != -1` at the top
of the loop and calls `ChangeCube` (`SOURCES/OBJECT.CPP`). `NewCube` is set by a zone
crossing (`GereZoneChangeCube`), a Life-script `LM_CHANGE_CUBE`, a phantom teleport,
or the console `cube` command.

`ChangeCube` does, in order:

1. `LastCubeMode = CubeMode`; seed RNG; `ClearScene()` (clears object data, not the
   framebuffer).
2. `LoadScene(NewCube)` (`SOURCES/DISKFUNC.CPP`). Early in `LoadScene`, after reading the
   header, it calls `FadeToBlackAndSamples(PtrPal)` then `Cls()`. Because the frame
   that triggered the change did `goto startloop` and skipped its own render, the
   `Log` buffer still holds the **fully-drawn old scene**, so this fade covers the
   old scene cleanly. Then the new cube's objects/palette load into the now-black
   screen.
3. Back in `ChangeCube`: `PtrInitGrille`, positioning, `RestartPerso`,
   `StartInitAllObjs`, and -- for a normal transition -- **`FlagFade = TRUE`**.
   `FirstTime = AFF_ALL_FLIP`; `NewCube = -1`.
4. Control returns to the main loop. The next `AffScene(FirstTime)` renders the new
   scene into `Log`, `BoxBlit` presents it (hardware palette still black), then the
   `AffScene` tail consumes `FlagFade` -> `FadeToPalAndSamples` reveals it.

So the fade-out is inside the load; the fade-in is the next frame's render tail.

## FMV transitions (`PlayAcf`)

FMVs are driven from `LM_PLAY_ACF` (`SOURCES/GERELIFE.CPP`), `TM_PLAY_ACF`
(`SOURCES/GERETRAK.CPP`), the visionneuse item (`PlayAllAcf`), and the menu (`BABY`,
`DELUGE`, `INTRO` in `SOURCES/GAMEMENU.CPP`). The in-game paths all do the same shape:

```
SaveTimer();
if (!FlagFade) FadeToBlackAndSamples(PtrPal);   // fade the current scene out
FlagFade = TRUE;
PlayAcf(name);                                   // play the video; leaves screen BLACK
if (CubeMode == CUBE_INTERIEUR) InitGrille(NumCube);
RazListPartFlow();
ChoicePalette();                                 // re-point PtrPal to the new palette
FlagFade = TRUE;
FirstTime = AFF_ALL_FLIP;
RestoreTimer();
```

The reveal is again the game loop's next `AffScene` consuming `FlagFade`.

### The `PlayAcf` contract (critical)

`PlayAcf` (`SOURCES/PLAYACF.CPP`) **fades the video out and leaves the screen black on
return** (its tail is `FadeToBlack(lastFramePal); Cls(); BoxStaticFullflip();
SetBlackPal();`). The caller must reveal the screen. Two reveal styles exist:

- **In-game caller** (`LM_PLAY_ACF`, `TM_PLAY_ACF`, `DELUGE`, `INTRO`): arms `FlagFade`;
  the game loop's `AffScene` fades in. `INTRO`'s caller sets `FlagFade` after
  `Introduction()` returns, so the opening scene fades up over the black load.
- **No fade-in loop** (`BABY` win screen, console `playvideo` preview): the caller
  reveals itself -- `BABY` does `PtrPal = PtrPalNormal; FadeToPal(PtrPalNormal)`;
  `playvideo` snapshots `Log`, plays, restores it, and `FadeToPal`s back.

A caller that neither arms `FlagFade` nor reveals itself will sit on black. That is
documented at the `PlayAcf` declaration in `SOURCES/PLAYACF.H`.

Two more `PlayAcf` facts worth knowing:
- It clears `FlagBlackPal` during playback (so the tail `FadeToBlack` actually ramps
  the last frame instead of early-returning), then sets it TRUE again at the tail.
- Its playback loop **freezes the frame while the console is open or the window has
  lost focus** (a never-focused run, e.g. a headless capture, keeps playing).

### The #404 regression

`a322185b` (the debug-console PR, #24) replaced the original leave-black tail with a
"restore the pre-video screen bright so we don't leave a black screen" band-aid.
That un-blacked the screen under every caller that expected black, so the caller's
`AffScene` presented the new scene **bright for one frame** before
`FadeToPalAndSamples` forced it black -- the pop. It is a timing race (the Smacker
decode runs on its own clock), so it is worse the higher the frame rate. PR #415
reverts `PlayAcf` to the original contract. The band-aid's real audience was the
bare `playvideo` preview, which now snapshots/restores itself instead.

## The reveal, in detail (`AffScene` tail)

At the end of `AffScene` (`SOURCES/OBJECT.CPP`), after `BoxBlit` has presented the
frame:

```
if (changepal)      PaletteSync(changepal);          // e.g. the lightning effect
if (FlagPal)      { PaletteSync(PtrPal); FlagPal=0; } // instant sync, no fade
if (FlagFade)     { FadeToPalAndSamples(PtrPal); FlagFade=0; }  // the fade-in reveal
else if (FlagLoadGame) PaletteSync(PtrPal);           // loaded save: instant
```

The scene is drawn into `Log` earlier in `AffScene`, and `BoxBlit` presents it *with
whatever palette is currently loaded*. For a clean reveal that palette must be
black at `BoxBlit` time, so the present shows black and only then does
`FadeToPalAndSamples` ramp it up. `FadeToPalAndSamples` always starts its ramp at
black (`delta = 0`), so the ramp itself is fine; the flash comes purely from the
one `BoxBlit` before it running at a non-black palette.

## The invariant (what "correct" looks like)

Fingerprint each present during a transition as `(content, brightness)`. A correct
transition is:

```
bright(old) ... -> ramp down -> black -> [content swap while black] ->
black(new) -> ramp up -> ... bright(new)
```

The rule: **no present shows changed or new content at a lit palette until the
fade-in ramp has started.** The black brackets the content swap on both sides. The
two failure modes are:

- **pop-out**: content changes (an actor torn down, the new scene composited) while
  the palette is still lit, before the fade-out covered it.
- **pop-in**: the new scene is shown lit before the fade-in ramps it up from black
  (the #404 flash).

## Regression classes seen so far

- **Wall-clock vs managed clock** (#353 / #374): fade timed on `SDL_GetTicks`
  collapses to one step across a load. Time fades on `TimerSystemHR`.
- **Un-blacked screen** (#404): a caller or `PlayAcf` leaves a lit palette when the
  new scene is first composited. Keep the palette black through the composite.
- **Object-layer wipe during a fade** (#417): `FadePal` re-presented each ramp step
  with `BoxUpdate`, whose inner `BoxClean` restores the clean, actor-less `Screen`
  backdrop over the moving (actor/sprite) boxes in `Log`. So the first fade step
  erased the composited object layer and the ramp faded terrain only: actors
  vanished at fade-out start and popped in after fade-in, on every scene load.
  Fixed by re-presenting with `BoxBlit` (present only). The rule: a fade re-presents
  the composited `Log`, it must never run `BoxClean` on the frame it is ramping.
- **Non-presenting fade** (#418): `FadePalToPal` (the lit-to-lit crossfade behind
  `FadeToPalIndex` / the `LM_FADE_TO_PAL` script opcode) rebuilt the palette LUT each
  step with `PaletteSync` but never re-presented, so under SDL the ramp uploaded no
  frames and the crossfade snapped straight to the target palette. About 10 scene
  scripts fire it on entry (island-arrival exteriors), visible in the attract reel.
  Fixed by presenting each step (`BoxStaticAdd` + `BoxBlit`) like the rest of the
  family. The rule: a palette ramp must re-present, not just `PaletteSync`.
- **Letterbox timer** (#354): the cutscene black bars (`FixeCinemaMode`, a clip
  window, not a palette fade) stuck then snapped; same subsystem, different
  mechanism.

## Observing a transition

An observability harness (present trace + oracle + grader) lives on the branch
`test/fade-transition-trace-404` (not shipped). It fingerprints every present with a
content-weighted luma and a palette-independent content hash. A correct transition
reads as monotonic luma ramps over a stable hash; a pop reads as a lit present whose
content hash already changed. Under `--fixed-dt` the capture is deterministic. The
records are emitted via `Log_Info` and reach stdout as `[control]`-prefixed copies
and stderr as `[INFO]` copies; the grader parses the `[INFO]` copies, so a capture
must be taken with `2>&1`.

## Key files

- `SOURCES/AMBIANCE.CPP` -- fade primitives, `ChoicePalette`, `SetBlackPal`.
- `SOURCES/OBJECT.CPP` -- `ChangeCube` (transition orchestration), `AffScene` (render +
  the `FlagFade`/`FlagPal` reveal tail).
- `SOURCES/DISKFUNC.CPP` -- `LoadScene` (the fade-out point).
- `SOURCES/PLAYACF.CPP` / `SOURCES/PLAYACF.H` -- `PlayAcf` (FMV playback + the
  leave-black contract), `PlayAllAcf`.
- `SOURCES/GERELIFE.CPP` / `SOURCES/GERETRAK.CPP` -- `LM_PLAY_ACF` / `TM_PLAY_ACF`.
- `SOURCES/GAMEMENU.CPP` -- menu FMVs (`BABY`, `DELUGE`, `INTRO`).
- `SOURCES/PERSO.CPP` -- `MainLoop` (where `ChangeCube` is triggered and `AffScene` runs).
- `LIB386/SVGA/SDL.CPP` -- present path, `paletteLUT`.

## Related

- `docs/TIMING.md` -- the clocks the fades ramp on.
- `docs/SCENES.md` -- scene/cube data and loading.
- `docs/MUSIC.md`, `docs/AUDIO.md` -- the sample/music side of the `AndSamples` fades.
