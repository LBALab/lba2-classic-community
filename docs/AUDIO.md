# Audio system

## Overview

The original LBA2 used the proprietary Miles Sound System (MSS) for audio. This project replaces Miles with an open-source SDL-based backend that implements the same AIL (Audio Interface Library) contract the engine was built against.

The SDL backend matches the AIL function signatures and handle semantics, but targets modern hardware and sound quality (44100 Hz, float mixing, clean clipping). This document describes the audio architecture, the AIL contract, how the engine uses audio, and current status.

For the music state machine specifically (track routing, the `PlayMusic` decision, the deferred-switch queue, and the stream backend's decode and park model), see [MUSIC.md](MUSIC.md).

**Legal note:** The SDL backend is a clean-room reimplementation of the AIL interface. The function signatures come from the game's own engine headers (`LIB386/H/AIL/`), not from decompilation or disassembly of Miles Sound System SDK. No proprietary RAD Game Tools / Miles code was reverse engineered.

---

## Retail audio assets (what's actually shipped)

What the original 1997 disc (and modern re-releases) contain at the data level. The "what asset is in what format" answer often surprises people because Miles Sound System was capable of MIDI synthesis, and this game's era was MIDI-heavy — but LBA2 itself doesn't use MIDI for any in-game music.

| Asset | Format | Location | Notes |
|---|---|---|---|
| Music | ADPCM-encoded WAV | `MUSIC/JADPCM01-18.WAV` (jingles, ~150 KB–1 MB each) and `MUSIC/TADPCM1-5.WAV` (themes, ~9–10 MB each), plus `MUSIC/LOGADPCM.WAV` (logo) | Steam transcoded these to OGG and ships them at `Common/Music/`. GOG ships the original WAVs inside `LBA2.GOG`'s ISO9660 filesystem. |
| Voice | ADPCM `.VOX` | `VOX/{LANG}_NNN.VOX` per language (DE / EN / FR / etc.) | Steam ships extracted in `Common/VOX/`; GOG `VOX/` directory is empty and the files live inside `LBA2.GOG`. |
| SFX | ADPCM samples | `SAMPLES.HQR` | Same archive on every distribution. |
| CD-DA track | One audio track | GOG ships as `LBA2.OGG` (track 2 of `LBA2.DAT` CUE) | The original retail CD also had this as a CD-DA track. The engine's `MUSIC.CPP` `TrackCD` table treats it as a CD-DA-style "track." |

**Loading GOG's in-image audio:** no extraction is required. The engine mounts `LBA2.GOG` and serves the in-image `MUSIC/` (jingles and themes) and `VOX/` through the normal asset path, and resolves the external `LBA2.OGG` track via the install's cue. See [DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md) for the loader and [GAME_DATA.md](GAME_DATA.md) for where to point the engine.

### Music is ADPCM, not MIDI

The `JADPCM` / `TADPCM` filename prefixes ("Jingle ADPCM", "Theme ADPCM") preserve the format lineage from 1997. Steam's `Common/Music/jadpcm01.ogg` etc. are direct OGG transcodes. Nothing in `SOURCES/MUSIC.CPP` ever invokes a MIDI playback API.

The MIDI driver-init code at `INITADEL.C:184` is real — `InitMidiDriver()` is called at startup — but for our default SDL backend the implementation in `LIB386/AIL/SDL/MIDI.CPP` is a no-op stub that just logs and returns `TRUE`. The Miles backend's `LIB386/AIL/MILES/MIDI.CPP` has working `AIL_start_sequence` calls, but no engine code path drives them with LBA2 content. The wiring exists because Miles Sound System bundled MIDI as part of its standard surface.

### The one MIDI artifact: `SETUP.MID`

A single Standard MIDI File exists across all retail distributions: `SETUP.MID` (~55 KB, format 1, 7 tracks), shipped only inside the GOG CD image at `/LBA2/SETUP.MID`. Its filename suggests installer use; the engine source has zero references to it (`grep -ri "SETUP.MID" SOURCES/ LIB386/` returns empty).

It renders cleanly through any General MIDI soundfont — a small piece of the era's auditory texture preserved on the disc but never played at runtime.

### Steam's `soundfont.sf2`

Steam ships a 32 MB `soundfont.sf2` at the install root and inside `Common/`. It's md5-identical to the community-distributed `GeneralUser-GS.sf2` (`bfe69fe5b2702ef7c12c44bd0d34f8f1`). The engine doesn't reference it at any code path; intent unclear.

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
    +-- SDL   (LIB386/AIL/SDL/)     -- recommended open-source backend
    +-- MILES (LIB386/AIL/MILES/)   -- original Adeline wrapper (requires proprietary Miles SDK)
    +-- NULL  (LIB386/AIL/NULL/)    -- silent stubs for testing
```

**Key principle: engine code changes are minimised.** The AIL headers define the contract. Any backend implements those headers. The CMake `SOUND_BACKEND` option (`null`, `miles`, `sdl`; default: `sdl`) selects which implementation is linked. This backend-abstraction pattern could be applied to LBA1, though its audio interface differs (separate L/R volumes, runtime DLL loading, MIDI for music). Engine-side changes are only made when the original code contains latent bugs that interact with the new backend (see [Engine-side fixes](#engine-side-fixes) below).

The debug console commands (`playsample`, `playmusic`, `audio sample/music/global`, etc.) call the same HQ/AIL/music functions the engine uses -- no wrapper layer, no indirection. Backend diagnostic logging can be toggled at runtime via `audio global log <0|1>`. Use `audio global status` to inspect `samplesPaused` ref-count and driver state without needing log mode.

The [platform abstraction layer](PLATFORM_ABSTRACTION.md) is a sibling of AIL, not another mixer or audio-device wrapper. `PLATFORM_BACKEND` selects graphics/input/timing/host services; `SOUND_BACKEND` independently selects AIL. The `sdl` sound backend uses the selected SDL2/SDL3 platform SDK. A non-SDL host selects the existing `null` backend or supplies an AIL implementation for its own audio facilities.

### SDL2 and SDL3 output transport

`--no-audio` (also implied by `--headless`) skips audio-subsystem initialization. Music and video playback check [AUDIO_DEVICE.CPP](../LIB386/AIL/SDL/AUDIO_DEVICE.CPP), `AILAudio_Initialized`, before resolving/decoding tracks or opening streams; video audio reports not started while video playback can continue silently. Existing samples remain gated by their driver state. Selecting the dummy driver alone does not disable audio: initialized dummy output still supports normal playback for tests. Initialization and device failures with audio enabled retain their diagnostics.

`test_sdl_audio_playback` exercises the real music/video paths with a synthetic WAV before initialization, after initialization with dummy output, and after shutdown. Disabled playback must leave audio uninitialized without logs or errors; enabled playback and invalid-specification diagnostics remain covered on both SDL SDKs.

The retained SDL AIL CD interface probes `music`, `Music`, and `MUSIC` below the working directory for `TrackNN.wav`. [CD.CPP](../LIB386/AIL/SDL/CD.CPP), `BuildCDTrackPath`, inserts a separator when the PAL's directory result has none; the contract does not require a trailing separator. This legacy lookup is not the gameplay music route, which uses the jingle/stream resolution chain.

[AUDIO_DEVICE.CPP](../LIB386/AIL/SDL/AUDIO_DEVICE.CPP) keeps device API differences private to AIL. Sample decoding, voice mixing, music decoding/parking, CD routing and video-audio policy are shared. The SDL3 path forwards to its device-stream API with unchanged source formats and callback behavior.

SDL2 drivers may allow only one open playback device. The SDL2 transport therefore owns one 44100 Hz stereo float device and mixes independently paused/gained streams into it. Each stream has an SDL converter from its source format/rate to that device format. A recursive mutex protects converters, stream membership and sample callbacks; opening and destroying streams belongs to the control thread, while the music decoder may enqueue on its worker thread. Destroying one stream leaves the others alive; destroying the last closes the device. Pausing one stream neither consumes its queue nor stops another stream.

SDL2 converts input when `SDL_AudioStreamPut` runs, unlike SDL3's playback-driven conversion. The SDL2 transport therefore copies submitted PCM into an identity-format input stream and feeds the output converter in bounded, frame-aligned chunks from the device callback. Loading a whole track does not convert it under the control thread's device lock or allocate its entire resampled float output up front. A flush reaches the converter only after all queued source PCM has been fed; queued-byte reporting includes the unconverted input. This transport applies to every SDL2 target, including Vita, Switch and desktop SDL2 builds; the SDL3 transport is unchanged.

WAV music uses a bounded producer on both SDL SDKs. [WAV_STREAM.CPP](../LIB386/AIL/SDL/WAV_STREAM.CPP), `AILWav_Open` and `AILWav_Read`, reads headers and one small prefill on the control thread, then the worker reads and decodes the rest in block-aligned batches through SDL's WAV decoder. Each submission is at most 16 KiB, with at most 64 KiB of source PCM queued; neither a full encoded track nor a full decoded track is retained. Mounted-image WAVs use [DISCIMG.CPP](../LIB386/SYSTEM/DISCIMG.CPP), `DiscImage_OpenIndependent`, so background reads have a private file cursor and cannot disturb game-asset reads. Stop joins the producer before destroying its stream; pause/focus nesting, seeked menu restore and natural-EOF flushing retain the music contracts described in [MUSIC.md](MUSIC.md). OGG caching and raw CD-DA loading are separate paths and do not have this WAV queue bound.

The streamed header parser follows SDL's `SDL_HINT_WAVE_RIFF_CHUNK_SIZE` policy: the default `ignorezero` accepts a zero RIFF size, `force` uses the declared size, and `ignore` / `maximum` search independently of it. RIFF size bounds the chunk search rather than a chunk's payload unless `SDL_HINT_WAVE_TRUNCATION=verystrict`; reads and payloads remain bounded by the physical file or image extent. The host WAV test compares decoded output byte-for-byte with SDL's whole-file loader under these policies.

SDL2's built-in resampler restarts its fractional phase at submission boundaries, so non-integer rate ratios can differ from a single whole-file conversion. Those rates are checked byte-for-byte against SDL's bounded streaming output; the existing 22050/44100 Hz cases retain whole-buffer equivalence checks.

The SDL2 minimum in [the platform CMake file](../LIB386/PLATFORM/CMakeLists.txt) is also a behavioral dependency, not just an API requirement. The SDL 2.0.22 resampler advances a floating-point time accumulator inside each conversion; splitting an identical source into bounded submissions restarts that accumulation, changing output bytes even for the game's integer 22050-to-44100 Hz ratio. Upstream's integer-phase implementation, present in SDL 2.28.0, calculates source positions and fractional remainders from integer frame counters instead. The supported minor-version floor selects that implementation consistently, rather than maintaining a patch-level exception for older SDL branches that received backports.

The distinction matters because submitting a whole track to SDL2's converter performs its entire conversion synchronously; doing so under the shared audio lock caused the measured multi-second music stalls. Returning to that submission pattern would restore the stall and full converted-output allocation. Supporting the older floating-phase resampler instead would require a different reference contract or a separate conversion implementation; neither is needed for the console SDKs used here. The bounded-conversion tests keep full byte comparisons, including whole-buffer equivalence at the game's integer rates. Non-integer ratios retain the explicitly bounded SDL reference described above.

The pinned Switch SDK supplies SDL 2.28.5 and the Vita build supplies SDL 2.32.8, so neither console needs an SDK change. Desktop SDL2 builds with an older package fail at configuration instead of encountering a conversion mismatch later. SDL3 remains the default desktop/Android backend and is unaffected. Linux CI reads the SDL2 floor from CMake and builds that genuine upstream release, covering both the public API and this resampling behavior.

The outer SDL2 callback [AUDIO_DEVICE.CPP](../LIB386/AIL/SDL/AUDIO_DEVICE.CPP), `Mix`, prepares crash handling through [CRASH.H](../LIB386/H/SYSTEM/CRASH.H), `Crash_PrepareThread`, before clearing output, locking or converting audio. This covers music/video-only playback and callbacks with every stream paused, independently of the nested sample callback. Preparation is idempotent per thread; the SDL3 sample callback retains its own preparation. The deterministic mixer regression checks preparation on every entry before output changes and preserves byte-exact silence and mixed output.

Queued-byte reporting remains in source units for music parking: the transport tracks submitted source frames minus consumed output frames scaled by the rate ratio. End-of-input flushing drains converter tails and clears remaining source-frame accounting when both input and converter are empty. Gain is applied to floating-point output before mixing, rather than quantized to SDL2's integer mixing-volume scale. `tests/platform/test_sdl_audio.cpp` verifies exact converter output over bounded reads, deferred conversion, pause/gain/queue behavior and simultaneous outputs; a separate production-callback test verifies dummy-device consumption.

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
| `SfxLogEnable` | `void SfxLogEnable(int enable)` | Toggle backend diagnostic logging (1=on, 0=off). |
| `SfxLogIsEnabled` | `int SfxLogIsEnabled(void)` | Query whether backend logging is on. |
| `GetSamplesPausedCount` | `S32 GetSamplesPausedCount(void)` | Query the `samplesPaused` ref-count (0 = running). |

**Struct:** `SAMPLE_PLAYING` -- per-voice state: `Usernum`, `Pitch`, `Repeat`, `Volume`, `Pan`, `Dummy`.

**Constants:** `MAX_SAMPLE_VOICES = 12`, `PITCHBEND_NORMAL = 4096`, `SAMPLE_RATE = 44100`, `MIX_CHUNK_FRAMES = 1024`. See [Sample rate and buffer design](#sample-rate-and-buffer-design) below.

### STREAM.H -- streaming audio (jingles, OGG)

| Function | Signature | Semantics |
|----------|-----------|-----------|
| `OpenStream` | `void OpenStream()` | Initialise the stream subsystem. |
| `PlayStream` | `void PlayStream(char *name)` | Stream a WAV/OGG file from disk. |
| `ChangeVolumeStream` | `void ChangeVolumeStream(S32 volume)` | Set stream volume (0-127). |
| `GetVolumeStream` | `S32 GetVolumeStream()` | Get current stream volume. |
| `StopStream` | `void StopStream()` | Stop playing stream. |
| `PauseStream` | `void PauseStream()` | Pause playing stream (parks the resume position). |
| `ResumeStream` | `void ResumeStream()` | Resume paused stream (from the parked position). |
| `SuspendStreamOutput` | `void SuspendStreamOutput()` | Device-level pause that does not touch the parked position (window focus loss). |
| `ResumeStreamOutput` | `void ResumeStreamOutput()` | Undo `SuspendStreamOutput`, unless the stream is logically paused. |
| `IsStreamPlaying` | `S32 IsStreamPlaying()` | TRUE while a stream is actively playing; FALSE when stopped or paused. |
| `StreamName` | `char *StreamName()` | Name of the current stream (empty if none). |
| `MusicLogEnable` | `void MusicLogEnable(int enable)` | Toggle the `[MUSIC]`/`[CD]` trace channel (1=on, 0=off). |
| `MusicLogIsEnabled` | `int MusicLogIsEnabled(void)` | Query whether the `[MUSIC]`/`[CD]` trace channel is on. |

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

Each sound backend implements this contract. SDL provides real audio; NULL and MILES provide silent stubs.

| Function | Signature |
|----------|-----------|
| `StartVideoAudio` | `S32 StartVideoAudio(S32 freq, S32 channels, S32 is16bit)` |
| `ResumeVideoAudio` | `void ResumeVideoAudio(void)` |
| `PauseVideoAudio` | `void PauseVideoAudio(void)` |
| `PushVideoAudio` | `void PushVideoAudio(const void *data, U32 sizeBytes)` |
| `StopVideoAudio` | `void StopVideoAudio(void)` |

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
- `CubeMode==CUBE_EXTERIEUR` AND `RAIN_FALLS` (COMMON.H): the chapter 0-1 storm on Citadel (`Island==0`), or a script's rain (`LM_PLUIE` sets `FlagRain`) on any island.
- Stopped when `CubeMode==CUBE_INTERIEUR` OR `!RAIN_FALLS`.

The original tested `TEMPETE_ACTIVE` (chapter < 2 OR `FlagRain`), which has no island term. Play cannot leave Citadel before chapter 2, but a debug cube warp can, and it then rained on other islands. `RAIN_FALLS` keeps the storm on Citadel and leaves script rain where the scripts put it: the three Zeelich ferry landings (cubes 97, 105, 132) play 5 seconds of rain with a lightning flash when the hero plays the ferryman's song (item 18).

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
| **Rain / storm** | Citadel Island (0) exterior in chapters 0-1; Zeelich ferry landings for 5 s when called | `SAMPLE_RAIN` overlay on ambience. |
| **Escalator buildings** | e.g. baggage claim (5, 6, 16) | Conveyor belt + ambience. |
| **Protection zones** | Various | Continuous alarm + other SFX. |
| **Buggy / Protopack** | Desert, exterior | `SampleAlways` engine/jetpack + ambience + possible music. |
| **Crowded interiors** | Bazaar (36), Bar (39), etc. | Multiple objects with `SampleAlways`, ambience slots. |

**Testing focus:** Desert exterior (e.g. cube 56 "Near the Camel", 60 "Near J. Baldino's House") for cicada loops; storm scenes for rain overlay; escalator scenes for conveyor anti-spam; protection zones for 3D panning of continuous alarm.

---

## Principles

- **API parity, modern implementation.** Match the AIL function signatures and handle semantics. Leverage modern hardware (44100 Hz, float mixing, clean clipping) rather than replicating 1990s limitations.

- **Correctness first.** Handle semantics, voice lifecycle, threading safety, and buffer management must be right before anything else.

- **Voice priority** (when 12 voices are exhausted): dialogue and UI first, then character/environmental SFX, then ambience loops, then flavour. Ambience creates mood but must never drown critical sounds.

- **Minimal engine changes.** Backend work stays in `LIB386/AIL/SDL/`. Engine files in `SOURCES/` are only modified for latent bugs exposed by the new backend, documented below.

---

## Known design notes

### `FadeOutVolumeMusic` / `FadeInVolumeMusic` asymmetry

`FadeOutVolumeMusic` uses `ChangeVolumeStream(jvolume)` (raw, no master scaling) because `GetVolumeStream()` returns a value that is already master-scaled from the last `SetVolumeJingle` call. `FadeInVolumeMusic` uses `SetVolumeJingle(jvolume)` (applies master scaling) because it interpolates toward the raw `JingleVolume` target. Both are correct; the asymmetry is intentional and avoids double-scaling.

### Master volume and stream coupling

In the original Miles wrapper (`LIB386/AIL/MILES/SAMPLE.CPP`), `SetMasterVolumeSample` called `ChangeVolumeStream(GetVolumeStream())` to re-apply stream volume whenever master changed. The SDL backend does NOT do this -- master volume for streams is applied at the engine level via the `SetVolumeJingle` macro (which embeds `MasterVolume` scaling). This means any call to `SetMasterVolumeSample` must be paired with `SetVolumeJingle(JingleVolume)` at the call site, or the stream volume won't update. All current call sites are covered (`ApplyMasterVolume`, which the settings table fires on a master write and `ReadVolumeSettings` calls at boot, and the master slider in `GAMEMENU.CPP`). Adding the coupling inside the backend would require including engine headers, violating the architecture.

### Music pause and resume are a reversible pair

`PauseMusic()` and `ResumeMusic()` (`SOURCES/MUSIC.CPP`) form a reversible pair, and the **backend owns the playback position**. The SDL backend keeps the paused device stream alive; a redbook/ISO backend re-seeks the saved sector. The engine layer must therefore never `Stop` on the resume path.

The reason is that `StopStream` is destructive on the SDL backend (output-stream destruction plus a decode-thread join), so a Stop during resume tears down the very stream it was meant to unpause, and `ResumeStream` then has nothing left to resume. The same Stop was a harmless no-op under MILES redbook -- `StopCD` only clears the track number, and `ResumeCD` replays from the saved position -- which is why the original Adeline code called `StopMusic()` first. That idiom did not survive the port: it silently killed the SDL stream, so music never came back after an in-game menu, Options screen, or dialogue cycle until the next `PlayMusic` (cube change, load, or scripted cue). See the `ResumeMusic` row under [Engine-side fixes](#engine-side-fixes).

**One stream, shared by CD and jingles.** In the SDL backend there is a single music stream. `PauseCD`/`StopCD` (`LIB386/AIL/SDL/CD.CPP`) forward to `PauseStream`/`StopStream`, and `IsCDPlaying()` is `cdPlaying && IsStreamPlaying()`. `PlayCD` is no longer reached: `PlayMusic` sends every entry through `PlayJingle`, so nothing sets `cdPlaying` and `GetMusic` answers from the stream's name instead. So the in-game menu theme and the gameplay music cannot coexist -- whichever plays last owns the one slot. This is why the in-game Options menu is the hard case: it plays the menu theme (`OptionsMenu`, under `#ifdef CDROM`) between the `PauseMusic` and `ResumeMusic` that bracket it (`PERSO.CPP`), so the menu theme's `PlayStream` tears down the paused gameplay stream. On a real CD the two could overlap because position lived in the drive, not in a stream object.

**How resume preserves position (remembered stream).** This restores a mechanism the original Adeline source already had and the SDL port had dropped. The original stream layer (`ail/STREAM.CPP` in the upstream Adeline tree) parked `PausedPos` + `PausedPathName` in `PauseStream`, tore the stream down, and resumed via `PlayThisStream(PausedPathName, PausedPos)` -- a reopen-and-seek, with `PlayStream(name)` being just `PlayThisStream(name, 0)`. The CD layer did the same with `Cur`/`End`/`PausedTrack` (preserved in our MILES wrapper, `LIB386/AIL/MILES/CD.CPP`). The SDL backend re-implements that contract on SDL + stb_vorbis:

- `PauseStream` snapshots `{streamPathName, frame}` and sets `hasRemembered`. The frame is `pushedBytes - AILAudio_Queued()` over `bytesPerFrame` (an approximation good to a device buffer; fine for music).
- The snapshot deliberately survives the `StopStream`/`PlayStream` of the menu theme -- it is cleared only by `ResumeStream` or `OpenStream`, exactly as MILES clears `PausedTrack` only in `ResumeCD`/`OpenCD`.
- `ResumeStream` has two paths: if the parked stream is still alive and paused (dialogue/holomap -- nothing played in between) it just unpauses the device; if a different stream replaced it (the Options menu theme) it re-opens the parked file and seeks back (`stb_vorbis_seek_frame` for OGG; a byte offset for a cached/WAV stream).

Two notes on fidelity to the original. First, the original `ResumeMusic` called `StopMusic()` before `ResumeCD`/`ResumeStream`, and it was harmless precisely because `StopStream`/`StopCD` never cleared the parked `PausedPathName`/`PausedTrack` -- so the reopen still found its saved position. The SDL port had naive device pause/resume with no parked position, so that inherited `StopMusic()` destroyed the only stream; this backend keeps the parked snapshot alive across a Stop, which is why removing `StopMusic()` from the engine's `ResumeMusic` (see the row below) is safe. Second, one deliberate divergence: the original *always* reopened on resume, whereas this backend keeps a fast path -- when nothing replaced the parked stream (dialogue/holomap) it just unpauses the live device. Same audible result, no redundant re-decode.

The engine layer is otherwise unchanged: `PauseMusic`/`ResumeMusic` issue only `Pause{CD,Stream}` / `Resume{CD,Stream}` with no intervening Stop, and `OptionsMenu` still plays the menu theme. The faithfulness lives entirely in the backend. The console exposes `audio music pause [fade 0|1]` / `audio music resume [fade 0|1]` (`SOURCES/CONSOLE/CONSOLE_CMD.CPP`) to drive the seam under the harness; note that `--exec` fires in one batch, so a harness-driven park reads `frame=0` (no elapsed playback), whereas real Options usage parks a large frame.

**For the planned ISO/BIN redbook backend:** keep the same contract -- `PauseCD` saves the sector, `ResumeCD` re-seeks it, and the snapshot survives a track change in between. The engine already assumes exactly this.

### Audio pauses when the window loses focus

Losing window focus pauses all audio and regaining it resumes from where it stopped (always on). This restores original behaviour the SDL port had dropped: the original `ApplicationInactive()` (WIN.CPP in the Adeline tree) paused the whole Miles output via `PauseMiles`/`ResumeMiles`, guarded by a `milespaused` flag, while it blocked the loop. The SDL port copied the timer half (`HandleEventsTimer` calls `LockTimer()` on `SDL_EVENT_WINDOW_FOCUS_LOST`, freezing the game clock) but not the audio half, so music kept playing on the independent SDL audio threads while the game was frozen.

Wiring:

- `HandleEventsWindow` (`LIB386/SYSTEM/WINDOW.CPP`) feeds the focus edge to `AppFocusAudio_OnChange()`.
- That policy lives in `LIB386/SYSTEM/FOCUS_AUDIO.CPP` -- deliberately SDL-free so its once-per-cycle guard (pause once on loss, resume once on gain, ignore repeated same-kind edges, like `milespaused`) is host-tested (`tests/focus_audio`).
- The audio layer registers the hooks (`InstallFocusAudioHooks`, called from `InitJingle`); `FocusLoseAudio`/`FocusGainAudio` (`SOURCES/AMBIANCE.CPP`) do the work.

The hooks pause samples and music, matching the original's pause-the-whole-output behaviour. The key design point is that these are **device-level** pauses, not the engine's logical pause. Music uses `SuspendStreamOutput` / `ResumeStreamOutput`, which pause the audio device without touching the parked resume position, so losing focus while an outer `PauseStream` is active (gameplay music parked while the Options menu theme plays) does not clobber that single-slot snapshot. `ResumeStreamOutput` resumes only when the stream is not logically paused, so a focus blip during a dialogue- or menu-driven music pause does not un-pause it. Samples use the ref-counted `Pause`/`ResumeSamples`, which compose with a menu that already paused them.

Fading is deliberately avoided (`PauseMusic(FALSE)`-style instant behaviour, never `HQ_PauseSamples` / a faded pause): focus loss locks the timer first (TIMER.CPP runs before WINDOW.CPP in the event pump), so a fade that waits on timer progress would never complete here.

A **playing cutscene** is not handled by these hooks. Its playback loop (`SOURCES/PLAYACF.CPP`) is paced on wall-clock (`SDL_GetTicks`), independent of the game timer, so it keeps advancing while the rest of the game is frozen. Pausing only its audio there would desync it against the still-advancing video. Instead the cutscene loop freezes itself when the window is unfocused (or the console is open): it holds the current frame, pauses the cutscene audio, and on resume re-bases its wall-clock start so playback does not fast-forward to "catch up". Video and its audio therefore pause and resume together.

### Sample rate and buffer design

**Sample rate (22 vs 44.1 kHz):** The original Miles backend used 22,050 Hz (`LIB386/AIL/MILES/COMMON.CPP`: `SamplingRate = 22050`), a 1990s-era choice for CPU and memory constraints. The SDL backend uses 44,100 Hz (CD standard) to target modern hardware and sound quality: better frequency response (Nyquist ~22 kHz vs ~11 kHz), float mixing, and clean clipping. Game assets (often 22 kHz or 11 kHz) are resampled up.

**Buffer duration:** Miles ran at 22 kHz with an effective buffer of ~512 samples per callback (~23 ms). When we upgraded to 44.1 kHz, keeping 512 frames would have halved the buffer duration to ~11.6 ms, increasing sensitivity to timing jitter. The SDL backend uses `MIX_CHUNK_FRAMES = 1024` so that 1024 samples at 44.1 kHz ≈ 23.2 ms, matching the original Miles buffer duration.

### WSL and audio jitter

Under WSL2 with WSLg, audio can stutter due to timing jitter. Known factors:

- **systemd-timesyncd:** Clock adjustments can cause PulseAudio timing jitter and stutter. Workaround: `sudo systemctl stop systemd-timesyncd` (or `disable` for persistence).
- **Recovery:** Once audio enters a jitter state, it may not recover; restarting the game is required.
- **Buffer:** The 1024-frame buffer helps mitigate timing jitter by providing more headroom than a smaller buffer would at 44.1 kHz.

---

## Diagnostic logging

Most audio diagnostic logging lives at the AIL boundary (`LIB386/AIL/SDL/`) and is toggled via the console. The one engine-layer exception is `SOURCES/MUSIC.CPP`, which emits high-level `[MUSIC]` transition lines (`PlayMusic`/`StopMusic`/`PauseMusic`/`ResumeMusic`) gated on the same `MusicLogIsEnabled()` toggle.

### Output

Backend lines use `SDL_Log()`; the engine-layer `[MUSIC]` lines use `Log_Info()`. Both reach the unified logger -- `LIB386/SYSTEM/LOG.CPP` captures `SDL_Log` via `SDL_SetLogOutputFunction` -- so everything lands in both the timestamped console output (stderr/NSLog) and the engine's `adeline.log`.

### Toggle

- **Console:** `audio global log <0|1>` toggles all backend logging at runtime.
- **Contract surface:** Two pairs gate logging, both part of the AIL contract: `SfxLogEnable`/`SfxLogIsEnabled` (SAMPLE.H) drive the `[SFX]` channel, and `MusicLogEnable`/`MusicLogIsEnabled` (STREAM.H) drive the `[MUSIC]`/`[CD]` channels. Because they are in the contract, every backend supplies them (the NULL and MILES backends provide trivial stubs). One toggle drives both: SDL's `SfxLogEnable` also calls `MusicLogEnable`, and the engine's `MUSIC.CPP` gates its own `[MUSIC]` lines on `MusicLogIsEnabled()`, so `audio global log 1` lights up the game-layer and backend lines together. The `MusicLog*` pair was promoted from SDL-private to the contract so MUSIC.CPP (engine layer) could gate its trace lines without depending on a backend-internal header.

### Coverage

Every public AIL function in the SDL backend is logged when enabled:

| File | Logged functions |
|------|-----------------|
| **SAMPLE.CPP** | `PlaySample` (ALLOC/STEAL/REJECTED), `SetMasterVolumeSample`, `ChangeVolumePanSample`, `ChangePitchbendSample`, `StopOneSample`, `StopSamples`, `PauseSamples`, `ResumeSamples`, `FadeOutSamples`, `FadeInSamples` |
| **STREAM.CPP** | `OpenStream`, `PlayStream`, `ChangeVolumeStream`, `StopStream`, `PauseStream`, `ResumeStream`, `SuspendStreamOutput`, `ResumeStreamOutput` |
| **CD.CPP** | `PlayCD`, `ChangeVolumeCD`, `StopCD`, `PauseCD`, `ResumeCD` |

Trivial getters (`IsSamplePlaying`, `GetVolumeCD`, `GetVolumeStream`, `IsCDPlaying`, `IsStreamPlaying`) are not logged to avoid noise.

### Tags

All log lines are prefixed with a channel tag for grep filtering:

- `[SFX]` -- sample operations (SAMPLE.CPP)
- `[MUSIC]` -- stream operations (STREAM.CPP)
- `[CD]` -- CD operations (CD.CPP)

---

## Engine-side fixes

These changes were made in engine code because they fix latent bugs exposed by the SDL backend's stricter semantics. Each is documented here with rationale.

### `SOURCES/AMBIANCE.CPP`

| Change | Original behaviour | Problem with SDL backend | Fix |
|--------|--------------------|--------------------------|-----|
| **`DelBlocHQRSample`: removed `HQ_StopSample()`** | Called `HQ_StopSample()` ("methode violente") on every HQR cache eviction, killing all playing voices. | SDL copies sample data (`malloc`+`memcpy`), so voices are independent of HQR cache entries. The blanket stop caused voice thrashing that silenced thunder and other concurrent SFX. | Removed `HQ_StopSample()`. Only `RestartRainSample = TRUE` remains to re-trigger rain after eviction. |
| **`ClearAmbiance`: added rain stop** | Only stopped the 4 `SampleAmbiance[]` slots. | With `DelBlocHQRSample` no longer killing all voices, rain could persist across cube transitions. | Added `HQ_StopOneSample(SAMPLE_RAIN)` before the ambiance slot loop. |

### `SOURCES/GAMEMENU.CPP`

Volume options are persisted in lba2.cfg; see [CONFIG.md](CONFIG.md).

| Change | Original behaviour | Problem with SDL backend | Fix |
|--------|--------------------|--------------------------|-----|
| **`new_game:` added `HQ_ResumeSamples()`** | The `new_game:` path ran `Introduction()` then entered `MainLoop()` without unpausing samples. | The SDL backend reference-counts `samplesPaused`. The menu calls `HQ_PauseSamples()` before showing. `PlayAcf` (inside `Introduction`) pairs its own pause/resume, but the menu's pause is never reversed, leaving `samplesPaused == 1` when `MainLoop()` starts — the mix callback returns early and all SFX are silent. The `load_game:` path already had the matching `HQ_ResumeSamples()`. | Added `HQ_ResumeSamples()` after `HQ_StopSample()`, matching `load_game:`. |
| **Case 70 else (resume from autosave) added `HQ_ResumeSamples()`** | After GameOver (`firstloop = TRUE`), selecting "reprendre partie" loaded CURRENTSAVE and entered `MainLoop()` without unpausing. | Same ref-counted `samplesPaused` issue: the menu's `HQ_PauseSamples()` is never reversed on this path, silencing all SFX. | Added `HQ_ResumeSamples()` before `RestoreTimer()`, matching `load_game:`. |
| **Master volume handler: added `SetVolumeJingle`** | Master volume slider only called `SetMasterVolumeSample` and `SetVolumeCD`. | All music routes through the jingle/stream path (not CD). Changing master volume had no effect on music because `SetVolumeJingle` was never called, and `SetVolumeCD` is a no-op when `cdPlaying` is false. | Added `SetVolumeJingle(JingleVolume)` to both left/right master volume cases so master volume scales music. |
|| **`GereVolumeMenu`: resume samples for SFX/Voice tests** | Volume menu entered while `samplesPaused > 0` (from `HQ_PauseSamples` when ESC opens menu from game). | The SDL backend pauses the sample audio device when `samplesPaused > 0`. SFX and Voice test sounds in the volume menu are allocated but inaudible because the device callback never runs. | Added `ResumeSamples()` on entry (when `GetSamplesPausedCount() > 0`) and matching `PauseSamples()` on exit. Restores `SetVolumeJingle(JingleVolume)` on exit to undo any CD slider override. |
|| **CD volume slider: added `SetVolumeJingle`** | CD slider called `SetVolumeCD(CDVolume)` only. | All music routes through jingles, so `cdPlaying` is always 0 and `ChangeVolumeCD` is a no-op. The CD test music plays as a jingle but adjusting CDVolume has no audible effect. | Added `SetVolumeJingle(CDVolume)` after `SetVolumeCD(CDVolume)` so the stream responds to the CD slider. |

### `SOURCES/MUSIC.H`

| Change | Original behaviour | Problem with SDL backend | Fix |
|--------|--------------------|--------------------------|-----|
| **`SetVolumeJingle` macro: added MasterVolume scaling** | `SetVolumeJingle(vol)` expanded to `ChangeVolumeStream(vol)` — no master scaling. In the original, jingles were short ADPCM clips; CD audio (the real background music) was scaled by master via `SetVolumeCD`. | All music now routes through jingle/stream (Steam/GOG has no CD). Without master scaling, jingle volume was independent of master — adjusting master volume didn't change music loudness. | Changed to `ChangeVolumeStream(RegleTrois(0, MasterVolume, 127, vol))`, matching `SetVolumeCD`'s pattern. |

**Miles backend note:** This macro change introduces double master-volume scaling if built with `SOUND_BACKEND=miles`, because the Miles wrapper's `ChangeVolumeStream` (`LIB386/AIL/MILES/STREAM.CPP`) already applies `LibWaveMasterVolume` scaling before passing the value to `AIL_set_stream_volume`. The SDL backend does not scale internally, so the macro-level scaling is correct for SDL. Since building with the Miles backend requires the proprietary RAD Game Tools SDK (unavailable for open-source builds), this is documented rather than fixed.

### `SOURCES/MUSIC.CPP`

| Change | Original behaviour | Problem with SDL backend | Fix |
|--------|--------------------|--------------------------|-----|
| **`TrackCD[6]`: added `JINGLE` flag** | Entry 6 (menu theme, music number 8) had no `JINGLE` flag, routing it through `PlayCD()`. | `PlayCD` constructs `Track08.wav` using physical CD track numbers. The Steam/GOG distribution names its music files `track1.ogg`–`track6.ogg` (sequential, no zero-padding). No filename permutation bridges this numbering gap. | Added `JINGLE` flag so the menu theme routes through `PlayJingle` → `PlayStream`, which has OGG fallback. |
| **`ListJingle[7]`: set to `"TADPCM6"`** | Empty string (`""`), because the menu theme was a physical CD audio track with no ADPCM stream. | With the `JINGLE` flag added above, `PlayJingle(8)` looks up `ListJingle[7]`, which was empty. | Set to `"TADPCM6"` following the existing TADPCM naming pattern. The stream layer's `track<N>.ogg` fallback resolves this to `track6.ogg`. |
| **`FadeOutVolumeMusic`: use `ChangeVolumeStream` directly** | Fade-out loop called `SetVolumeJingle(jvolume)` where `jvolume` was derived from `GetVolumeStream()` (already a raw stream value). | With `SetVolumeJingle` now applying MasterVolume scaling, the intermediate fade values would be double-scaled. | Changed to `ChangeVolumeStream(jvolume)` for the fade loop and final zero-set, avoiding double scaling. |
| **`ResumeMusic`: removed `StopMusic()`** | `ResumeMusic` called `StopMusic()` before `ResumeCD()`/`ResumeStream()` inside `#ifdef CDROM`, which the shipping build defines (`SOURCES/CMakeLists.txt`). | `StopStream` is destructive on the SDL backend (`SDL_DestroyAudioStream` + thread join), so the Stop tore down the paused stream and `ResumeStream` had nothing to resume. Music went silent after any in-game Pause/Options/dialogue cycle until the next `PlayMusic`. The Stop was a harmless no-op on MILES redbook (`ResumeCD` replays from the saved position), which is why the original code had it. | Removed `StopMusic()`; `ResumeCD()` + `ResumeStream()` alone are correct for every backend. See [Music pause and resume are a reversible pair](#music-pause-and-resume-are-a-reversible-pair). Guarded by a host-only spy-backend regression test (`tests/music/test_music_pause_resume.cpp`) that asserts `ResumeMusic` issues `Resume{CD,Stream}` and never `Stop{CD,Stream}`. |

### `SOURCES/AMBIANCE.CPP`

| Change | Original behaviour | Problem with SDL backend | Fix |
|--------|--------------------|--------------------------|-----|
| **`ReadVolumeSettings`: reordered volume application** | `SetVolumeJingle(JingleVolume)` was called before `MasterVolume` was read from config. | With `SetVolumeJingle` now scaling by MasterVolume, calling it before MasterVolume is loaded from config would use the wrong master value. | Moved `SetVolumeJingle` and `SetVolumeCD` calls to after `MasterVolume` is read and `SetMasterVolumeSample` is called. |
