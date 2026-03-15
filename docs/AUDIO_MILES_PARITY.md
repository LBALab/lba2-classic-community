# SDL Audio -- AIL Contract & Miles Parity

## Goal

The original LBA2 used the proprietary **Miles Sound System (MSS)** to implement its audio. This project replaces Miles with an open-source **SDL-based backend** that implements the same **AIL (Audio Interface Library)** contract the game was built against.

The goal is **parity and correctness**: the SDL backend must faithfully reproduce what Miles did for this game -- same function signatures, same handle semantics, same audible behaviour. A lot of the work is already done; this document defines the contract, tracks what needs verification, and guides testing.

---

## Architecture

The game's audio call chain is layered and already has a clean abstraction boundary:

```
Engine code (SOURCES/*.CPP)
    |  calls HQ functions and music/jingle functions
    v
HQ layer (AMBIANCE.CPP, MUSIC.CPP)
    |  game-level logic: 3D pan, volume scaling, "always" samples,
    |  music/jingle lifecycle, fade timing
    v
AIL functions (LIB386/AIL/)
    |  backend boundary -- swapped at compile time via SOUND_BACKEND
    |
    +-- SDL   (LIB386/AIL/SDL/)     -- current default, what we are hardening
    +-- MILES (LIB386/AIL/MILES/)   -- original proprietary implementation (reference)
    +-- NULL  (LIB386/AIL/NULL/)    -- silent stubs for testing
```

**Key principle: engine code is never modified for backend work.** The AIL headers define the contract. Any backend implements those headers. The CMake `SOUND_BACKEND` option (`null`, `miles`, `sdl`) selects which implementation is linked. This design is also reusable for LBA1 (same AIL interface).

The debug console commands (`playsample`, `playmusic`, `audio sample/music/global`, etc.) call the same HQ/AIL/music functions the engine uses -- no wrapper layer, no indirection.

---

## The AIL contract

These are the C headers every backend must implement. They live in `LIB386/H/AIL/`.

### SAMPLE.H -- sample playback (SFX, ambience, voices)

| Function | Signature | Semantics |
|----------|-----------|-----------|
| `InitSampleDriver` | `S32 InitSampleDriver(char *driver_name)` | Initialise the sample subsystem. |
| `SetMasterVolumeSample` | `void SetMasterVolumeSample(S32 volume)` | Set master volume (0-127) for all samples. |
| `PlaySample` | `U32 PlaySample(void *buffer, S32 sizeBytes, U32 userhandle, S32 pitchbend, S32 repeat, S32 volume, S32 pan)` | Play a sample from a memory buffer. Returns a handle. |
| `ChangePitchbendSample` | `void ChangePitchbendSample(U32 sample, S32 pitchbend)` | Change pitchbend on a playing sample. `4096` = no bend. |
| `ChangeVolumePanSample` | `void ChangeVolumePanSample(U32 sample, S32 volume, S32 pan)` | Change volume (0-127) and pan (0-127, 64=centre) on a playing sample. |
| `StopOneSample` | `void StopOneSample(U32 sample)` | Stop a single sample by handle. |
| `StopSamples` | `void StopSamples(void)` | Stop all playing samples. |
| `PauseSamples` | `void PauseSamples(void)` | Pause all playing samples. |
| `ResumeSamples` | `void ResumeSamples(void)` | Resume all paused samples. |
| `IsSamplePlaying` | `U32 IsSamplePlaying(U32 sample)` | Check if a sample is playing. Returns `FALSE` or starting `TimerRefHR`. |
| `FadeOutSamples` | `S32 FadeOutSamples(S32 delay)` | Fade out all samples over `delay` ms. Returns scaling factor. |
| `FadeInSamples` | `S32 FadeInSamples(S32 delay)` | Fade in all samples over `delay` ms. Returns scaling factor. |
| `InverseStereoSample` | `void InverseStereoSample(S32 inverse)` | Swap left/right channels when `inverse` is TRUE. |

**Struct:** `SAMPLE_PLAYING` -- per-voice state: `Usernum`, `Pitch`, `Repeat`, `Volume`, `Pan`, `Dummy`.

**Constants:** `MAX_SAMPLE_VOICES = 12`, `PITCHBEND_NORMAL = 4096`, `SAMPLE_RATE = 22050`.

### STREAM.H -- streaming audio (jingles, OGG)

| Function | Signature | Semantics |
|----------|-----------|-----------|
| `OpenStream` | `void OpenStream()` | Initialise the stream subsystem. |
| `PlayStream` | `void PlayStream(char *name)` | Stream a WAV/OGG file from disk. |
| `ChangeVolumeStream` | `void ChangeVolumeStream(S32 volume)` | Set stream volume (0-127). |
| `GetVolumeStream` | `S32 GetVolumeStream()` | Get current stream volume. |
| `StopStream` | `void StopStream()` | Stop playing stream. |
| `PauseStream` | `void PauseStream()` | Pause playing stream. |
| `ResumeStream` | `void ResumeStream()` | Resume paused stream. |
| `IsStreamPlaying` | `S32 IsStreamPlaying()` | TRUE if a stream is playing or paused. |
| `StreamName` | `char *StreamName()` | Name of the current stream (empty if none). |

### CD.H -- CD audio

| Function | Signature | Semantics |
|----------|-----------|-----------|
| `OpenCD` | `const char* OpenCD(char *volume_name)` | Initialise CD. Returns drive letter. |
| `PlayCD` | `void PlayCD(S32 track)` | Play CD track (1-based). |
| `ChangeVolumeCD` | `void ChangeVolumeCD(S32 volume)` | Set CD volume (0-127). |
| `GetVolumeCD` | `S32 GetVolumeCD()` | Get current CD volume. |
| `StopCD` | `void StopCD()` | Stop CD playback. |
| `PauseCD` | `void PauseCD()` | Pause CD playback. |
| `ResumeCD` | `void ResumeCD()` | Resume CD playback. |
| `IsCDPlaying` | `S32 IsCDPlaying()` | TRUE if CD is playing or paused. |

### VIDEO_AUDIO.H -- video soundtrack (smacker)

| Function | Signature |
|----------|-----------|
| `StartVideoAudio` | `S32 StartVideoAudio(S32 freq, S32 channels, S32 is16bit)` |
| `ResumeVideoAudio` | `void ResumeVideoAudio(void)` |
| `PushVideoAudio` | `void PushVideoAudio(const void *data, U32 sizeBytes)` |
| `StopVideoAudio` | `void StopVideoAudio(void)` |

---

## What the game actually uses from Miles

We only care about parity for **what LBA2 actually calls**, not the full MSS feature set.

- **12 sample voices** (`MAX_SAMPLE_VOICES`) with per-voice volume (0-127), pan (0-127, 64=centre), pitchbend (4096=normal), looping (0=infinite, >0=count), and handle tracking.
- **Handle encoding**: `PlaySample` returns a 32-bit handle. `IsSamplePlaying`, `ChangePitchbendSample`, `ChangeVolumePanSample`, and `StopOneSample` accept both real handles and synthetic `numsample << 8` handles used by "always" sample tracking.
- **Master volume**, **pause/resume**, **fade in/out**, and **reverse stereo** for all samples.
- **Stream playback** for jingles (OGG/WAV files via `PlayStream`).
- **CD audio** for music tracks (or CD-to-file emulation via `PlayCD`).
- **Separate volume controls** for SFX, music/jingles, and CD.
- **Audio formats**: RIFF WAV (PCM and IMA ADPCM), Creative Voice (.voc), raw 8-bit unsigned PCM. The SDL backend handles all of these.

---

## Sound scripting patterns

This section maps how the engine uses the AIL API in gameplay. It guides targeted testing and helps identify mix-sensitive scenes.

### 1. 3D mix calls (`HQ_3D_MixSample`, `HQ_3D_ChangePanSample`)

**Where:** INVENT, FICHE, BUGGY, IMPACT, EXTRA, OBJECT, PERSO, GERELIFE, GERETRAK, DART.

**Flow:**
1. `HQ_3D_MixSample(numsample, frequence, decalage, repeat, x, y, z)` projects 3D position to screen via `PtrProjectPoint`.
2. `GiveBalance(Xp, Yp, distance, &pan, &volume)` computes pan (0–127) and volume (0–127):
   - **Interior:** `distance = Distance2D(320, 240, Xp, Yp)`; volume falls off within `DISTANCE_SAMPLE_MAX` (700).
   - **Exterior:** `distance = Distance3D(x,y,z, CameraX,Y,Z)`; volume full within `DISTANCE_SAMPLE_MIN_EXT` (5000), fades to 0 by `DISTANCE_SAMPLE_MAX_EXT` (30000).
3. Calls `HQ_MixSample` with computed pan/volume.
4. If `GiveBalance` returns FALSE (off-screen) and `repeat==0`, returns synthetic handle `numsample<<8` for "always" samples.

**Typical parameters:** `frequence=0x1000` (normal pitch), `decalage=0` or 300–2000 (random pitch), `repeat=0` (loop) or `1` (one-shot).

### 2. Always-playing samples (`SampleAlways`)

**Where:** FICHE (object scripts), BUGGY, PERSO, GERELIFE (`LM_SAMPLE_ALWAYS`), GERETRAK (`TM_SAMPLE_ALWAYS`).

**Pattern:**
- Stored in `ptrobj->SampleAlways` (handle or `numsample<<8`).
- **Repanning:** `SampleAlwaysMove` timer (every ~2 s) triggers `HQ_3D_ChangePanSample` when the object moves.
- **Restart:** If `!IsSamplePlaying(SampleAlways)`, call `HQ_3D_MixSample` with `repeat=0`.
- **Sample change:** If new sample ID differs, `HQ_StopOneSample` old handle first.
- **Stop:** `HQ_StopOneSample(SampleAlways)` when object deactivates (e.g. get off buggy, stop protopack).

**Examples:**
- **Buggy engine** (`SAMPLE_BUGGY`): looping, `ChangePitchbendSample` for speed.
- **Protopack** (`SAMPLE_PROTOPACK`): looping while flying.
- **Object scripts:** ambient loops (machinery, water, etc.) tied to objects.

### 3. Ambience loops (`LaunchAmbiance`, `GereAmbiance`)

**Data source:** Scene file per cube. `SampleAmbiance[0..3]`, `SampleRepeat[0..3]`, `SampleRnd`, `SampleFreq`, `SampleVol`, `SecondMin`, `SecondEcart` loaded in `DISKFUNC.CPP`.

**LaunchAmbiance (on cube enter):**
- For each slot `n` with `SampleAmbiance[n] != -1` and `repeat==0`: start infinite loop immediately via `HQ_MixSample(numsample, freq, decal, 0, 64, volume)`.

**GereAmbiance (per-frame):**
- When `TimerNextAmbiance` fires: pick random slot, if `repeat!=0` and `!IsSamplePlaying(numsample)`, play one-shot with random pan.
- `SecondMin` + `SecondEcart` control interval between one-shots.

**ClearAmbiance (on cube change):** `HQ_StopOneSample` for each defined slot.

**Mix-sensitive:** Desert exterior cubes (White Leaf Desert, Island 2) typically use cicada-like samples in `SampleAmbiance` slots. Interior cubes may use machinery, water drips, etc.

### 4. Rain sample (`SAMPLE_RAIN`)

**Where:** `AMBIANCE.CPP` — `StartRainSample`, `ChoicePalette`, `GereRain`-related paths.

**Conditions:**
- `CubeMode==CUBE_EXTERIEUR` AND `!TEMPETE_FINIE` (storm active).
- Stopped when `CubeMode==CUBE_INTERIEUR` OR `TEMPETE_FINIE`.

**Pattern:** `HQ_MixSample(SAMPLE_RAIN, 0x1000, 300, 0, 64, volume)` — infinite loop, center pan, decalage 300 for variation.

### 5. Escalator (`SAMPLE_ESCALATOR`)

**Where:** `OBJECT.CPP` — `CJ_ESCALATOR_OUEST/EST/NORD/SUD`.

**Pattern:** `if (!IsSamplePlaying(SAMPLE_ESCALATOR)) HQ_MixSample(SAMPLE_ESCALATOR, 0x1000, 0, 1, 64, VOLUME_ESCALATOR)`.
- One-shot (`repeat=1`) but re-triggered each frame while on conveyor. Anti-spam via `IsSamplePlaying` + `SAMPLE_TIME_REPEAT` in `HQ_MixSample`.
- Non-3D, center pan.

### 6. Non-3D one-shots

**Examples:**
- `PlayErrorSample` macro: `SAMPLE_ERROR`, center pan, volume 127.
- `SAMPLE_BONUS`, `SAMPLE_FOUND_OBJ`, `SAMPLE_GAME_OVER`: UI/feedback.
- `SAMPLE_DEAD`, `SAMPLE_EXPLO_*`, `SAMPLE_BONUS_TROUVE`: 3D-positioned in EXTRA/OBJECT.

### 7. Protection alarm (INVENT)

**Pattern:** 3D-positioned `SAMPLE_PROTECTION` / `SAMPLE_PROTECTION_ALARME`, always-playing when zone active. `HQ_3D_ChangePanSample` on position update.

---

## Mix-sensitive scenes

| Scene type | Cubes | Notes |
|------------|-------|------|
| **Desert exterior** | 55–73, 198–220 (White Leaf Desert) | `SampleAmbiance[0..3]` from scene file — cicadas, wind, ambient. Often multiple infinite loops. |
| **Rain / storm** | Exterior cubes when `TEMPETE_ACTIVE` | `SAMPLE_RAIN` overlay on ambience. |
| **Escalator buildings** | e.g. baggage claim (5, 6, 16) | Conveyor belt + ambience. |
| **Protection zones** | Various | Continuous alarm + other SFX. |
| **Buggy / Protopack** | Desert, exterior | `SampleAlways` engine/jetpack + ambience + possible music. |
| **Crowded interiors** | Bazaar (36), Bar (39), etc. | Multiple objects with `SampleAlways`, ambience slots. |

**Testing focus:** Desert exterior (e.g. cube 56 "Near the Camel", 60 "Near J. Baldino's House") for cicada loops; storm scenes for rain overlay; escalator scenes for conveyor anti-spam; protection zones for 3D panning of continuous alarm.

---

## Principles

These guide how we evaluate and tune the SDL backend:

- **Parity over novelty.** Where behaviour is audible, match what Miles did: relative loudness, distance attenuation, panning, looping, transitions.

- **Correctness first.** Handle semantics, voice lifecycle, threading safety, and buffer management must be right before we optimise or add features.

- **Clarity and headroom.** Dialogue and critical SFX must stay intelligible over ambience and music. Ambience creates mood but should not dominate.

- **Smooth transitions.** No pops or harsh on/off at sample start/end/loop points, pause/resume, or music track changes. Short fades are preferred where they do not break gameplay timing.

- **Priority tiers** (when voices are exhausted):
  1. Dialogue, UI feedback, critical gameplay cues.
  2. Character actions, important environmental SFX.
  3. Ambience loops (cicadas, rain, machinery).
  4. Flavour/secondary SFX.

- **Zero engine file changes.** Backend work happens in `LIB386/AIL/SDL/`. The engine source files in `SOURCES/` are never modified for audio backend purposes.

---

## Console test scenarios

The debug console exercises the same HQ/AIL functions the engine uses. These scenarios verify the SDL backend:

- **Ambience + SFX bed** -- Start looping ambience (`playsample <id> 0x1000 0 0 80 64`), overlay SFX with additional `playsample` calls. Listen for clean loop points, intelligible SFX, non-fatiguing ambience.

- **Crowded SFX burst** -- Rapidly trigger many samples to hit the 12-voice limit. Confirm no crashes, no stuck "playing" flags, higher-priority sounds still come through.

- **Dynamic pitch** -- Start a looping sample (`repeat=0`), then test pitch changes from gameplay code (`ChangePitchbendSample`). Check for smooth transitions, no aliasing artefacts.

- **3D panning** -- Move through a scene with 3D audio sources (`HQ_3D_MixSample` / `HQ_3D_ChangePanSample`). Confirm left/right pan tracks screen position, distance attenuation matches inside/outside behaviour.

- **Pause/resume** -- Start ambience and music, use `audio global pause` / `audio global resume` and `audio music stop` / `audio music play`. Use `audio global reset` if state gets confused. Confirm smooth transitions, no audio blasts on resume.

- **Music + heavy SFX** -- Start music (`playmusic <track>`), overlay heavy SFX. Confirm music stays audible, overall mix does not clip or distort.

---

## Audit: Miles vs SDL deviations (found and fixed)

A function-by-function comparison of `LIB386/AIL/MILES/SAMPLE.CPP` against `LIB386/AIL/SDL/SAMPLE.CPP` was performed. The following deviations were found and corrected.

### SAMPLE.CPP fixes applied

| # | Deviation | Miles behaviour | SDL before | Fix applied |
|---|-----------|----------------|------------|-------------|
| 1 | **FadeOutSamples / FadeInSamples** | Full fade state machine: records `FadeStartTime`, computes `delta`, scales all voice volumes from stored base volume via `ScaleVolumeSamples(ratio)`. `FadeOut` returns ms remaining (0 = done). `FadeIn` returns ms elapsed. During fade, `PlaySample` mutes new sounds. | Stubs returning 0 / delay. No volume scaling, no fade flag. | Implemented fade state machine with `samplesFade` flag, `fadeStartTime`, and `ScaleVolumeSamples(ratio)` using `baseVolume` field. New sounds muted during fade-out. |
| 2 | **IsSamplePlaying return value** | Returns `FALSE` or `Sample_Handle[hnum].timer` (the `TimerSystemHR` when the voice started). Engine uses this as a timestamp for anti-spam in `HQ_MixSample`: `(TimerSystemHR - timesample) < SAMPLE_TIME_REPEAT`. | Returned the handle value, not a timer. Anti-spam check in `HQ_MixSample` was broken. | Added `timer` field to Voice struct, set to `TimerSystemHR` in `PlaySample`. `IsSamplePlaying` now returns `voices[i].timer`. |
| 3 | **Voice stealing priority** | Three-pass priority: (a) steal oldest one-shot (`repeat==1`), (b) steal oldest finite-loop (`repeat!=0`), (c) steal oldest infinite-loop. "Oldest" = lowest handle value (monotonic generation counter). | Always evicted slot 0 regardless of priority or age. | Implemented three-pass priority matching Miles: one-shots first, then finite-loops, then any. Oldest by handle value. |
| 4 | **Output clipping** | Miles internal mixer handles clipping. | No clipping. Voices summed with `+= (S16)(...)`, wrapping on overflow with 12 voices. | Mix into float accumulators (`mixL[]`, `mixR[]`), clamp to `[-32768, 32767]` before writing S16 output. |
| 5 | **Thread safety** | Miles manages threading internally; wrapper code runs single-threaded. | No synchronisation. `SampleStreamCallback` (SDL audio thread) and main thread both access `voices[]` without locks. Race conditions on play/stop/modify. | Added `SDL_Mutex *voiceMutex`. Locked in callback and all main-thread voice operations (`PlaySample`, `StopOneSample`, `StopSamples`, `ChangePitchbendSample`, `ChangeVolumePanSample`, `IsSamplePlaying`, `SetMasterVolumeSample`, `FadeOutSamples`, `FadeInSamples`). |
| 6 | **SetMasterVolumeSample scaling** | Does `volume++` to shift 0-127 → 0/2-128 for clean 7-bit fixed-point scaling. | Stored raw value 0-127. Slight volume curve difference. | Added `volume++` trick matching Miles. |
| 7 | **Pan clamping** | Pan clamped to 0-127 in both `PlaySample` and `ChangeVolumePanSample`. | Pan only clamped on the high end (> 127), not low end (< 0). | Added lower bound clamp: `(pan < 0) ? 0 : pan`. |
| 8 | **Volume suppression during fade** | When `SamplesFade == TRUE`, `PlaySample` sets volume to 0 for new sounds. | No fade flag existed. New sounds during fade played at full volume. | Check `samplesFade` in `PlaySample`, set volume to 0 when true. `baseVolume` stores the intended volume for post-fade restoration. |

### STREAM.CPP fixes applied

| # | Deviation | Miles behaviour | SDL before | Fix applied |
|---|-----------|----------------|------------|-------------|
| 9 | **IsStreamPlaying after audio drains** | Checks `AIL_stream_status == SMP_DONE` and auto-cleans (calls `StopStream`). Returns FALSE when finished. | Only checked `stream != NULL`. After audio data drained, stream pointer stayed non-NULL, so returned TRUE forever. | Now checks `SDL_GetAudioStreamQueued(stream)`. If no data queued, calls `StopStreamInternal()` and returns FALSE. |
| 10 | **StopStream while paused** | `StopStream` closes the stream regardless of pause state. | Returned early if `streamPaused == true`, making `StopStream` a no-op while paused. | Removed the early return. `StopStream` now always stops. |

### Remaining observations (not yet addressed)

| # | Item | Notes |
|---|------|-------|
| A | **Stereo source handling** | For stereo WAVs, `stepSamples` is doubled but the mixer reads single-indexed samples, effectively playing only the left channel. Miles handles stereo correctly. LBA2 game assets are predominantly mono, so impact is likely low. |
| B | **Pan law difference** | SDL uses a piecewise linear pan where center (64) gives both channels at 1.0 (0 dB). Miles uses its own internal pan law which likely attenuates center by ~3 dB (equal-power). This makes centered mono sounds ~6 dB louder in the SDL mix vs hard-panned. |
| C | **Focus-loss stutter** | When the window loses focus for extended periods, audio can stutter. A hard reinit mechanism (teardown + recreate SDL audio device) may be needed. |
| D | **No shutdown/cleanup** | `InitSampleDriver` has no counterpart. Mutex and audio stream are never explicitly destroyed. |

---

## Parity checklist

For the SDL backend, and for any future backend, we must be able to say "yes" to all of these:

**Samples:**
- [x] 12 voices, sensible handling under voice exhaustion (priority-based stealing).
- [x] Handle semantics (`PlaySample` / `IsSamplePlaying` / `Change*` / `StopOneSample`) match, including "always" sample synthetic handles.
- [x] `IsSamplePlaying` returns `TimerSystemHR` (not handle), matching Miles anti-spam contract.
- [x] Pitchbend formula matches Miles: `newRate = baseRate * pitchbend / 4096`.
- [x] Volume and pan behave correctly, including reverse stereo and pan clamping.
- [x] Looping and repeat semantics match (0=infinite, >0=count), end-of-sample fade.
- [x] Pause/resume reference-counted, matching Miles nesting semantics.
- [x] Fade in/out state machine implemented, matching Miles timing and volume suppression.
- [x] Master volume uses `volume++` trick for clean 7-bit fixed-point scaling.
- [x] Thread safety: voice mutex protects all shared state between audio and main threads.
- [x] Output clipping: float accumulator with S16 clamp prevents overflow distortion.
- [x] Audio formats (WAV PCM, WAV IMA ADPCM, VOC, raw 8-bit) all decode correctly.

**Music / CD / Stream:**
- [x] Play/stop/loop calls produce correct audible behaviour.
- [x] `IsStreamPlaying` correctly detects end-of-stream (checks queued data).
- [x] `StopStream` works regardless of pause state.
- [x] SFX, music, and CD volumes track their respective sliders.
- [ ] Transitions between tracks are free of pops (needs audible verification).
- [ ] Music/CD never make critical SFX or dialogue unintelligible (needs audible verification).

**Robustness:**
- [ ] No stutter or breakage on window focus loss/gain (known issue C).
- [x] No buffer underruns during normal gameplay (mixer clipping prevents distortion).
- [ ] Mixer state can be inspected or logged to diagnose audio bugs (future: add voice dump command).

---

## Known issues

- **Focus-loss audio stutter/breakage.** When the game window loses focus for extended periods (e.g. console open for a while), audio can stutter and break. The current `audio global reset` (soft reset) does not fix this. A hard reinit mechanism (teardown + recreate SDL audio device) is needed.

- **Stereo source playback.** Stereo WAV samples only play the left channel due to the step-doubling approach in the mixer. Game assets are predominantly mono so this is low priority but should be fixed for correctness.

- **Pan law at center.** SDL gives +6 dB at center vs hard-panned. Miles likely used equal-power panning (~-3 dB at center). The overall mix balance will differ slightly from the original.

---

## Implementation plan

**Phase 1 -- Console test hooks** *(done)*
- [x] Debug console audio commands (`playsample`, `playmusic`, `playjingle`, `audio sample/music/global`) call HQ/AIL/music functions directly.
- [x] Console commands wired into build.

**Phase 2 -- SDL backend hardening** *(in progress)*
- [x] Audit Miles vs SDL implementation function by function.
- [x] Fix fade in/out state machine.
- [x] Fix `IsSamplePlaying` return value (timer, not handle).
- [x] Fix voice stealing priority (one-shot → finite-loop → infinite-loop, oldest first).
- [x] Fix output clipping (float accumulators + S16 clamp).
- [x] Add thread safety (voice mutex).
- [x] Fix `SetMasterVolumeSample` scaling.
- [x] Fix `IsStreamPlaying` end-of-stream detection.
- [x] Fix `StopStream` while paused.
- [ ] Implement hard reinit mechanism (teardown + recreate SDL audio device/streams), exposed via `audio global reinit` console command.
- [ ] Audit and fix the focus-loss stutter bug.
- [x] Map sound scripting patterns (3D mix, ambience loops, SampleAlways, mix-sensitive scenes).
- [ ] Add instrumentation/logging: voice allocation/deallocation, callback timing.
- [ ] Fix stereo source playback in the mixer.
- [ ] Consider equal-power pan law.
- [ ] Work through remaining parity checklist items with console test scenarios.

**Phase 3 -- Alternative backends (future)**
- [ ] Prototype an SDL3_mixer backend under `LIB386/AIL/SDL3MIXER/` implementing the same AIL headers.
- [ ] Verify against the parity checklist.
- [ ] Evaluate the Miles backend (`LIB386/AIL/MILES/`) as a comparison reference.
