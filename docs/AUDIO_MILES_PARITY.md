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

## Parity checklist

For the SDL backend, and for any future backend, we must be able to say "yes" to all of these:

**Samples:**
- [ ] 12 voices, sensible handling under voice exhaustion.
- [ ] Handle semantics (`PlaySample` / `IsSamplePlaying` / `Change*` / `StopOneSample`) match, including "always" sample synthetic handles.
- [ ] Pitchbend is smooth across gameplay range (buggy engine, etc.).
- [ ] Volume and pan behave correctly, including reverse stereo.
- [ ] Looping and repeat semantics match (0=infinite, >0=count), clean loop points.
- [ ] Pause, resume, and fade functions work correctly at HQ level.
- [ ] Audio formats (WAV PCM, WAV IMA ADPCM, VOC, raw 8-bit) all decode correctly.

**Music / CD / Stream:**
- [ ] Play/stop/loop calls produce correct audible behaviour.
- [ ] SFX, music, and CD volumes track their respective sliders.
- [ ] Transitions between tracks are free of pops.
- [ ] Music/CD never make critical SFX or dialogue unintelligible.

**Robustness:**
- [ ] No stutter or breakage on window focus loss/gain.
- [ ] No buffer underruns during normal gameplay.
- [ ] Mixer state can be inspected or logged to diagnose audio bugs.

---

## Known issues

- **Focus-loss audio stutter/breakage.** When the game window loses focus for extended periods (e.g. console open for a while), audio can stutter and break. The current `audio global reset` (soft reset) does not fix this. A hard reinit mechanism (teardown + recreate SDL audio device) is needed.

---

## Implementation plan

**Phase 1 -- Console test hooks** *(done)*
- [x] Debug console audio commands (`playsample`, `playmusic`, `playjingle`, `audio sample/music/global`) call HQ/AIL/music functions directly.
- [x] Console commands wired into build.

**Phase 2 -- SDL backend hardening**
- [ ] Add instrumentation/logging in `LIB386/AIL/SDL/SAMPLE.CPP`: voice allocation/deallocation, buffer underruns, callback timing, format decoding.
- [ ] Implement hard reinit mechanism (teardown + recreate SDL audio device/streams), exposed via `audio global reinit` console command.
- [ ] Audit and fix the focus-loss stutter bug.
- [ ] Review each AIL contract function in the SDL implementation against the semantics documented above. Track findings per-function.
- [ ] Work through the parity checklist, verifying each item with console test scenarios.

**Phase 3 -- Alternative backends (future)**
- [ ] Prototype an SDL3_mixer backend under `LIB386/AIL/SDL3MIXER/` implementing the same AIL headers.
- [ ] Verify against the parity checklist.
- [ ] Evaluate the Miles backend (`LIB386/AIL/MILES/`) as a comparison reference.
